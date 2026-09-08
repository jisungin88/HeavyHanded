#include "Hazards/MovementTrap.h"

#include "Character/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Noise/NoiseSubsystem.h"
#include "TimerManager.h"

AMovementTrap::AMovementTrap()
{
	PrimaryActorTick.bCanEverTick = false;

	// Multicast_PlayTriggerEffect 를 쓰려면 복제 액터여야 한다
	bReplicates = true;

	TrapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrapMesh"));
	SetRootComponent(TrapMesh);
	TrapMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// 밟았는지만 보면 되므로 폰 채널만 오버랩으로 연다. 나머지 채널을 다 막아 두는 것은
	// 노획물을 던져서 맞혀도 반응하지 않게 하기 위함이다 — 덫은 발로 밟아야 걸린다.
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(TrapMesh);
	TriggerVolume->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AMovementTrap::OnTriggerOverlap);
}

void AMovementTrap::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "밟았는데 아무 일도 안 일어난다" 하나로만 드러난다 (ABreakableWall::BeginPlay 와 동일 사유)
	if (!IsValid(TrapMesh) || !TrapMesh->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[MovementTrap:%s] TrapMesh 에 메시가 없다 — 눈에 안 보이는 덫이 된다"), *GetName());
	}

	if (!IsValid(TriggerEffect))
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[MovementTrap:%s] TriggerEffect 가 비어 있다 — 걸려도 연출이 안 나온다"), *GetName());
	}
}

void AMovementTrap::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 서버 권위 판정. 재무장 전이면 무시한다
	if (!HasAuthority() || !bArmed)
	{
		return;
	}

	// AGuardCharacter 는 별도 클래스라 여기 안 걸린다 — 경비가 자기 구역 덫에 스스로 안 걸리는 이유
	ABaseCharacter* Target = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(Target))
	{
		return;
	}

	UCharacterMovementComponent* Movement = Target->GetCharacterMovement();
	if (!IsValid(Movement))
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[MovementTrap:%s] %s 에 CharacterMovementComponent 가 없다 — 가두지 못한다"),
			*GetName(), *Target->GetName());
		return;
	}

	bArmed = false;

	// MOVE_None — 중력도 함께 멈춘다. 바닥에 서 있는 상태로 걸리는 것을 전제로 한다
	// (AMovementTrap 헤더의 "⚠️ 부작용" 참고)
	Movement->DisableMovement();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReleaseTimer,
			FTimerDelegate::CreateUObject(this, &AMovementTrap::ReleaseTarget, TWeakObjectPtr<ABaseCharacter>(Target)),
			ImmobilizeDuration, false);

		// 재무장은 풀려난 뒤로부터가 아니라 걸린 시점부터 ImmobilizeDuration + ResetDelay 뒤다 —
		// 타이머 두 개를 따로 재는 것보다 이쪽이 걸리는 순간 기준으로 한 번에 이해하기 쉽다
		World->GetTimerManager().SetTimer(RearmTimer, this, &AMovementTrap::Rearm,
			ImmobilizeDuration + ResetDelay, false);
	}

	// 소음은 이 사건 자체가 발생원이다. Noise.Hazard.Trap 은 Noise.ini(지성인)에 이미
	// 등록된 태그를 참조만 한 것 — 새로 만들지 않았다
	if (UNoiseSubsystem* Noise = UNoiseSubsystem::Get(this))
	{
		Noise->ReportNoise(HHTags::Noise_Hazard_Trap, GetActorLocation(), 1.f, Target);
	}

	Multicast_PlayTriggerEffect();

	UE_LOG(LogHazard, Log, TEXT("[MovementTrap:%s] %s 를 %.1f초간 가뒀다"),
		*GetName(), *Target->GetName(), ImmobilizeDuration);
}

void AMovementTrap::ReleaseTarget(TWeakObjectPtr<ABaseCharacter> TargetPtr)
{
	ABaseCharacter* Target = TargetPtr.Get();
	if (!IsValid(Target))
	{
		// 풀어주기 전에 대상이 사라졌다(체포·접속 종료 등) — 그럼 풀어줄 것도 없다
		return;
	}

	if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
}

void AMovementTrap::Rearm()
{
	if (bDestroyAfterTrigger)
	{
		// 바나나 껍질 같은 소모성 함정 — 다시 무장하는 대신 사라진다.
		// bReplicates = true 라서 서버의 Destroy() 가 클라에도 정상적으로 반영된다
		Destroy();
		return;
	}

	bArmed = true;
}

void AMovementTrap::Multicast_PlayTriggerEffect_Implementation()
{
	UWorld* World = GetWorld();

	// 데디케이티드 서버는 화면도 스피커도 없다 (ABreakableWall::ApplyBreach 와 동일 사유)
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (IsValid(TriggerEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, TriggerEffect, GetActorLocation());
	}

	if (IsValid(TriggerSound))
	{
		UGameplayStatics::PlaySoundAtLocation(World, TriggerSound, GetActorLocation());
	}
}
