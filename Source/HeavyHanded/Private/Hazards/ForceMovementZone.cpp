#include "Hazards/ForceMovementZone.h"

#include "Character/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hazards/HazardLog.h"

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
	if (!HasAuthority() || Occupants.IsEmpty())
	{
		return;
	}

	const FVector Offset = GetActorForwardVector() * ForceSpeed * DeltaTime;

	for (ABaseCharacter* Occupant : Occupants)
	{
		if (IsValid(Occupant))
		{
			// 스윕을 켜서 미는 도중 벽을 뚫지 않게 한다
			Occupant->AddActorWorldOffset(Offset, /*bSweep=*/true);
		}
	}
}

void AForceMovementZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	// AGuardCharacter 는 안 걸린다 — 다른 Hazard 클래스들과 동일 사유
	ABaseCharacter* Target = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(Target))
	{
		return;
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

	ABaseCharacter* Target = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(Target))
	{
		return;
	}

	Occupants.Remove(Target);

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
