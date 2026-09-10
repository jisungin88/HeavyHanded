#include "Hazards/PressurePlate.h"

#include "AbilitySystemComponent.h"
#include "Alert/AlertComponent.h"
#include "Character/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"   // FGameplayTag::RequestGameplayTag — State.ShadowStep 임시 문자열 조회
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "Loot/LootBase.h"

APressurePlate::APressurePlate()
{
	PrimaryActorTick.bCanEverTick = false;

	// Multicast_PlayAlarmSound 를 쓰려면 복제 액터여야 한다
	bReplicates = true;

	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
	SetRootComponent(PlateMesh);

	// 시각 전용이다. 바닥은 레벨의 실제 지오메트리가 담당한다 (ACreakyFloor::FloorMesh 와 동일 사유)
	PlateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 다른 Hazard 클래스들과 같은 이유로 폰 채널만 오버랩으로 연다
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(PlateMesh);
	TriggerVolume->SetBoxExtent(FVector(50.f, 50.f, 30.f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &APressurePlate::OnTriggerOverlap);
}

void APressurePlate::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "밟았는데 아무 일도 안 일어난다" 하나로만 드러난다 (다른 Hazard 클래스와 동일 사유)
	if (!IsValid(PlateMesh) || !PlateMesh->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[PressurePlate:%s] PlateMesh 에 메시가 없다 — 눈에 안 보이는 압력판이 된다"), *GetName());
	}
}

void APressurePlate::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 서버 권위 판정 (다른 Hazard 클래스들과 동일 사유)
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

	// 그림자 이동 중엔 무시한다 — Config/Tags/State.ini 의 State.ShadowStep 코멘트에 이미
	// "압력판 무시" 라고 명시돼 있다. 네이티브 선언 대신 문자열 조회를 쓰는 이유는
	// HeavyHandedGameplayTags.h 를 환경 방해 요소 작업 동안 건드리지 않기로 한 방침 때문이다.
	if (UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent())
	{
		static const FGameplayTag ShadowStepTag = FGameplayTag::RequestGameplayTag(TEXT("State.ShadowStep"));
		if (ASC->HasMatchingGameplayTag(ShadowStepTag))
		{
			return;
		}
	}

	// 빈손이거나 싼 물건이면 반응하지 않는다
	ALootBase* HeldLoot = Cast<ALootBase>(Target->GetHeldActor());
	if (!IsValid(HeldLoot) || HeldLoot->GetCurrentValue() < ValueThreshold)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (LastTriggerTime >= 0.f && Now - LastTriggerTime < MinRetriggerInterval)
	{
		// 오버랩 경계에서 스치듯 들락거린 것 — 진짜 재진입이 아니다 (ACreakyFloor 와 동일 사유)
		return;
	}
	LastTriggerTime = Now;

	// 세계 경계도를 직접 올린다. SetAlertGauge01 은 "치트 · 스크립트 이벤트용" 으로 이미
	// 열려 있는 통로다(AlertComponent.h) — 소음 태그를 새로 만들 필요가 없다.
	if (UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		Alert->SetAlertGauge01(FMath::Clamp(Alert->GetAlertGauge01() + AlertGaugeIncrease, 0.f, 1.f));
	}

	Multicast_PlayAlarmSound();

	// 경보 다음에 놓친다 — 순서 자체에 필연적 이유는 없지만, 플레이어가 "경보가 울렸다"를
	// 먼저 인지해야 무슨 일이 일어났는지 바로 이해한다
	Target->SetHeldActor(nullptr);

	UE_LOG(LogHazard, Log, TEXT("[PressurePlate:%s] %s 가 $%d 짜리 %s 를 들고 지나감 — 경보"),
		*GetName(), *Target->GetName(), HeldLoot->GetCurrentValue(), *GetNameSafe(HeldLoot));
}

void APressurePlate::Multicast_PlayAlarmSound_Implementation()
{
	const UWorld* World = GetWorld();

	// 데디케이티드 서버는 화면도 스피커도 없다 (다른 Hazard 클래스들과 동일 사유)
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (IsValid(AlarmSound))
	{
		UGameplayStatics::PlaySoundAtLocation(World, AlarmSound, GetActorLocation());
	}
}
