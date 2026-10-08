#include "Hazards/ForceMovementZone.h"

#include "Character/BaseCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "Loot/LootBase.h"

AForceMovementZone::AForceMovementZone()
{
	// 다른 Hazard 클래스와 다르게, 안에 있는 동안 계속 밀어야 해서 Tick 이 필요하다
	PrimaryActorTick.bCanEverTick = true;

	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	SetRootComponent(ZoneMesh);

	// 발판이라 실제로 밟고 서야 한다 (덫/웅덩이/마루의 장식 전용 메시와 다른 점)
	ZoneMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// 다른 Hazard 클래스들과 같은 이유로 폰 채널만 오버랩으로 연다
	ForceVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ForceVolume"));
	ForceVolume->SetupAttachment(ZoneMesh);
	ForceVolume->SetBoxExtent(FVector(100.f, 100.f, 50.f));
	ForceVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ForceVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ForceVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 노획물은 Pawn 이 아니라 커스텀 오브젝트 채널 "Loot"(ECC_GameTraceChannel2, DefaultEngine.ini
	// Profiles=(Name="Loot",...) 참고)을 쓴다. 기본 응답이 Block 이고 위에서 전부 Ignore 로
	// 돌려놨으니, 노획물과 오버랩을 잡으려면 이 채널도 따로 열어야 한다.
	ForceVolume->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);
	ForceVolume->SetGenerateOverlapEvents(true);
	ForceVolume->OnComponentBeginOverlap.AddDynamic(this, &AForceMovementZone::OnZoneBeginOverlap);
	ForceVolume->OnComponentEndOverlap.AddDynamic(this, &AForceMovementZone::OnZoneEndOverlap);
}

void AForceMovementZone::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "위에 서 있는데 아무 일도 안 일어난다" 하나로만 드러난다 (다른 Hazard 클래스와 동일 사유)
	if (!IsValid(ZoneMesh) || !ZoneMesh->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[ForceMovementZone:%s] ZoneMesh 에 메시가 없다 — 눈에 안 보이는 장치가 된다"), *GetName());
	}
}

void AForceMovementZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 서버 권위 — 미는 판정은 서버에서만 한다 (다른 Hazard 클래스들과 동일 사유)
	if (!HasAuthority() || (Occupants.IsEmpty() && LootOccupants.IsEmpty()))
	{
		return;
	}

	const FVector ForwardDir = GetActorForwardVector();
	const FVector Offset = ForwardDir * ForceSpeed * DeltaTime;

	for (ABaseCharacter* Occupant : Occupants)
	{
		if (!IsValid(Occupant))
		{
			continue;
		}

		// 걷기든 점프든, 벨트를 거스르는 쪽 속도 성분을 매 틱 지운다.
		// AddActorWorldOffset 만으로는 "그냥 더하는" 구조라 자기 이동 속도가 ForceSpeed 보다
		// 빠르면(특히 점프 중 에어 컨트롤) 그대로 뚫고 지나간다 — 반대 방향 성분만 0으로
		// 되돌려서 벨트가 항상 이기게 한다. 옆/앞 방향 이동은 그대로 자유롭다.
		if (UCharacterMovementComponent* Movement = Occupant->GetCharacterMovement())
		{
			const FVector Velocity = Movement->Velocity;
			const float OpposingSpeed = FVector::DotProduct(Velocity, -ForwardDir);
			if (OpposingSpeed > 0.f)
			{
				Movement->Velocity = Velocity + ForwardDir * OpposingSpeed;
			}
		}

		// 스윕을 켜서 미는 도중 벽을 뚫지 않게 한다
		Occupant->AddActorWorldOffset(Offset, /*bSweep=*/true);
	}

	for (ALootBase* Loot : LootOccupants)
	{
		if (!IsValid(Loot) || Loot->GetPrimaryCarrier() != nullptr)
		{
			// 누가 집어드는 순간 밀기를 멈춘다 — EndOverlap 이 아직 안 왔어도 여기서 걸러진다
			continue;
		}

		UPrimitiveComponent* PhysicsRoot = Loot->GetPhysicsRoot();
		if (!IsValid(PhysicsRoot) || !PhysicsRoot->IsSimulatingPhysics())
		{
			continue;
		}

		// 캐릭터처럼 순간이동(AddActorWorldOffset)시키면 물리가 깨진다 — 속도를 축별로
		// 분해해서 맞춘다. 전진축은 ForceSpeed 로 고정, 위아래(중력·바운스)는 물리에 맡기되,
		// 좌우(옆) 성분은 매 틱 지운다 — 안 지우면 작고 가벼운 노획물이 충돌로 생기는
		// 미세한 옆 방향 튐에도 속도 변화가 커서(질량이 작을수록 같은 충격량에 속도 변화가
		// 크다) 드리프트가 누적되다 벨트 밖으로 밀려난다.
		const FVector RightDir = GetActorRightVector();
		const FVector Velocity = PhysicsRoot->GetPhysicsLinearVelocity();
		const float RightSpeed = FVector::DotProduct(Velocity, RightDir);
		const FVector VerticalVelocity = Velocity - ForwardDir * FVector::DotProduct(Velocity, ForwardDir) - RightDir * RightSpeed;
		PhysicsRoot->SetPhysicsLinearVelocity(VerticalVelocity + ForwardDir * ForceSpeed);
	}
}

void AForceMovementZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	// AGuardCharacter 는 안 걸린다 — 다른 Hazard 클래스들과 동일 사유. 그림자 이동 중에도
	// 밀리게 두는 게 맞다고 판단해 ShadowStep 은 원래부터 안 가렸다(변경 없음)
	ABaseCharacter* Target = nullptr;
	if (!IsValidHazardTarget(OtherActor, Target, /*bCheckShadowStep=*/false))
	{
		// 캐릭터가 아니면 노획물인지 확인한다 — 둘 다 아니면(장식물 등) 무시
		if (ALootBase* Loot = Cast<ALootBase>(OtherActor))
		{
			// 들려있는 중이면 플레이어 쪽 이동에 이미 딸려가므로 따로 밀지 않는다
			if (Loot->GetPrimaryCarrier() == nullptr)
			{
				if (Occupants.IsEmpty() && LootOccupants.IsEmpty())
				{
					Multicast_StartRunningSound();
				}
				LootOccupants.AddUnique(Loot);
			}
		}
		return;
	}

	// 첫 탑승자일 때만 재생 시작 — 이미 돌아가는 중이면 또 틀 필요 없다
	if (Occupants.IsEmpty() && LootOccupants.IsEmpty())
	{
		Multicast_StartRunningSound();
	}
	Occupants.AddUnique(Target);

	// 튜브처럼 저항 자체를 없애는 장치는 본인 조작을 끊는다.
	// AMovementTrap 과 같은 방식(DisableMovement) — 무브먼트 모드는 CMC 가 알아서 복제한다.
	// 우리 자신의 강제 이동(Tick 의 AddActorWorldOffset)은 무브먼트 모드와 무관하게 계속 먹는다.
	if (bOverrideControl)
	{
		if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
		{
			Movement->DisableMovement();
		}
	}
}

void AForceMovementZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	ABaseCharacter* Target = nullptr;
	if (!IsValidHazardTarget(OtherActor, Target, /*bCheckShadowStep=*/false))
	{
		if (ALootBase* Loot = Cast<ALootBase>(OtherActor))
		{
			LootOccupants.Remove(Loot);
			if (Occupants.IsEmpty() && LootOccupants.IsEmpty())
			{
				Multicast_StopRunningSound();
			}
		}
		return;
	}

	Occupants.Remove(Target);

	// 마지막 탑승자가 나갔을 때만 정지
	if (Occupants.IsEmpty() && LootOccupants.IsEmpty())
	{
		Multicast_StopRunningSound();
	}

	// 끊었던 조작을 되돌린다. ⚠️ 알려진 한계 — 겹치는 강제 이동 존 두 개를 동시에 지나가면
	// 먼저 나간 쪽이 조작을 되돌려서, 아직 다른 존 안에 있는데도 잠깐 조작이 풀릴 수 있다.
	// Base 버전에서는 이 정도로 두고, 존이 겹치는 배치가 실제로 필요해지면 다시 다룬다.
	if (bOverrideControl)
	{
		if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

void AForceMovementZone::Multicast_StartRunningSound_Implementation()
{
	const UWorld* World = GetWorld();

	// 데디케이티드 서버는 화면도 스피커도 없다 (PlayHazardSound 와 동일 사유)
	if (!World || World->GetNetMode() == NM_DedicatedServer || !IsValid(RunningSound))
	{
		return;
	}

	// 이미 재생 중이면 다시 만들지 않는다 (ASecurityCamera::PlayAlarmSound 와 동일 사유)
	if (IsValid(RunningAudioComponent) && RunningAudioComponent->IsPlaying())
	{
		return;
	}

	RunningAudioComponent = UGameplayStatics::SpawnSoundAttached(RunningSound, ZoneMesh);
}

void AForceMovementZone::Multicast_StopRunningSound_Implementation()
{
	if (IsValid(RunningAudioComponent))
	{
		RunningAudioComponent->Stop();
	}
	RunningAudioComponent = nullptr;
}
