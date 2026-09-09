#include "Hazards/SlipTrap.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Character/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Engine/World.h"
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

ASlipTrap::ASlipTrap()
{
	PrimaryActorTick.bCanEverTick = false;

	// Multicast_PlaySlipEffect 를 쓰려면 복제 액터여야 한다
	bReplicates = true;

	PeelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PeelMesh"));
	SetRootComponent(PeelMesh);

	// 시각 전용이다. 밟아서 막히면 안 되므로 콜리전을 끈다 (APuddle::PuddleMesh 와 동일 사유)
	PeelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 다른 Hazard 클래스들과 같은 이유로 폰 채널만 오버랩으로 연다
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(PeelMesh);
	TriggerVolume->SetBoxExtent(FVector(40.f, 40.f, 30.f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ASlipTrap::OnTriggerOverlap);
}

void ASlipTrap::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "밟았는데 아무 일도 안 일어난다" 하나로만 드러난다 (다른 Hazard 클래스들과 동일 사유)
	if (!IsValid(PeelMesh) || !PeelMesh->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[SlipTrap:%s] PeelMesh 에 메시가 없다 — 눈에 안 보이는 함정이 된다"), *GetName());
	}
}

void ASlipTrap::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 서버 권위 판정 (다른 Hazard 클래스들과 동일 사유)
	if (!HasAuthority())
	{
		return;
	}

	// AGuardCharacter 는 안 걸린다 — 경비가 자기 순찰 중 스스로 미끄러지지 않는다
	ABaseCharacter* Target = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(Target))
	{
		return;
	}

	// 이미 다운된 대상을 또 넘어뜨릴 이유가 없다 — 복구 중인 사람이 다시 넉백당하면
	// 복구 로직과 부딪힐 수 있다
	if (Target->IsDowned())
	{
		return;
	}

	// ── 1. 넉백이 먼저다 — 아직 정상 이동 모드일 때 걸어야 씹히지 않는다 ──
	const FRotator RandomYaw(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
	const FVector SlipDirection = RandomYaw.RotateVector(FVector::ForwardVector);
	const FVector LaunchVelocity = SlipDirection * SlipLaunchStrength
		+ FVector::UpVector * (SlipLaunchStrength * SlipUpwardRatio);

	Target->LaunchCharacter(LaunchVelocity, true, true);

	// ── 2. 들고 있던 것을 놓친다 ──
	Target->SetHeldActor(nullptr);

	// ── 3. 다운 상태로 전환 — AVanZone::SendLoadedEvent 와 같은 방식으로 이벤트만 보낸다.
	//        실제 판정(태그 부여·복구 로직)은 캐릭터 파트의 GAB_Downed 가 갖고 있다.
	FGameplayEventData Payload;
	Payload.EventTag = HHTags::Event_Player_Downed;
	Payload.Instigator = this;
	Payload.Target = Target;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, HHTags::Event_Player_Downed, Payload);

	Multicast_PlaySlipEffect();

	UE_LOG(LogHazard, Log, TEXT("[SlipTrap:%s] %s 가 밟고 미끄러짐 — 다운 이벤트 발송"),
		*GetName(), *Target->GetName());

	if (bDestroyAfterTrigger)
	{
		if (DestroyDelay > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(DestroyTimerHandle, this, &ASlipTrap::DestroySelf, DestroyDelay, false);
		}
		else
		{
			DestroySelf();
		}
	}
}

void ASlipTrap::DestroySelf()
{
	Destroy();
}

void ASlipTrap::Multicast_PlaySlipEffect_Implementation()
{
	const UWorld* World = GetWorld();

	// 데디케이티드 서버는 화면도 스피커도 없다 (다른 Hazard 클래스들과 동일 사유)
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (IsValid(SlipEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SlipEffect, GetActorLocation());
	}

	if (IsValid(SlipSound))
	{
		UGameplayStatics::PlaySoundAtLocation(World, SlipSound, GetActorLocation());
	}
}
