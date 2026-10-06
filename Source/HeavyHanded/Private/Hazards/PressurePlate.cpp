#include "Hazards/PressurePlate.h"

#include "Alert/AlertComponent.h"
#include "Alert/AlertSettings.h"
#include "Character/BaseCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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

	// AGuardCharacter 는 안 걸린다 — 다른 Hazard 클래스들과 동일 사유. 그림자 이동 중에도
	// 무시한다 — Config/Tags/State.ini 의 State.ShadowStep 코멘트에 이미 "압력판 무시"
	// 라고 명시돼 있다(AHazardBase::IsValidHazardTarget 참고)
	ABaseCharacter* Target = nullptr;
	if (!IsValidHazardTarget(OtherActor, Target))
	{
		return;
	}

	// 빈손이거나 싼 물건이면 반응하지 않는다
	ALootBase* HeldLoot = Cast<ALootBase>(Target->GetHeldActor());
	if (!IsValid(HeldLoot) || HeldLoot->GetCurrentValue() < ValueThreshold)
	{
		return;
	}

	if (!ShouldRetrigger(MinRetriggerInterval))
	{
		return;
	}

	// 세계 경계도를 직접 올린다. SetAlertGauge01 은 "치트 · 스크립트 이벤트용" 으로 이미
	// 열려 있는 통로다(AlertComponent.h) — 소음 태그를 새로 만들 필요가 없다.
	if (UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		const float Scaled = AlertGaugeIncrease * UAlertSettings::Get()->GaugeIncreaseScale;
		Alert->SetAlertGauge01(FMath::Clamp(Alert->GetAlertGauge01() + Scaled, 0.f, 1.f));
	}

	Multicast_PlayAlarmSound();

	// 경보 다음에 손상시킨다 — 순서 자체에 필연적 이유는 없지만, 플레이어가 "경보가
	// 울렸다"를 먼저 인지해야 무슨 일이 일어났는지 바로 이해한다.
	//
	// ReportImpact 는 게이팅 없이 확정 충격 하나를 그대로 방송한다(헤더 주석 참고) —
	// ULootDurabilityComponent(파손형)가 붙어 있으면 금이 가고, 없으면 조용히 아무 일도
	// 안 일어난다. LootBase.h/.cpp 는 건드리지 않는다 — 이미 공개된 함수만 호출한다.
	HeldLoot->ReportImpact(ELootImpactCause::Collision, LootDamageImpulse,
		HeldLoot->GetActorLocation(), Target);

	UE_LOG(LogHazard, Log, TEXT("[PressurePlate:%s] %s 가 $%d 짜리 %s 를 들고 지나감 — 경보"),
		*GetName(), *Target->GetName(), HeldLoot->GetCurrentValue(), *GetNameSafe(HeldLoot));
}

void APressurePlate::Multicast_PlayAlarmSound_Implementation()
{
	// 데디케이티드 서버 가드 (PlayHazardSound 와 동일 사유)
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !IsValid(AlarmSound))
	{
		return;
	}

	// 이전 재생이 아직 끝나기 전에 재발동하면 소리가 겹친다 — 밟고 나갔다가 금방
	// 다시 밟는 경우가 실제로 있다(ASecurityCamera 에서 이미 마주친 문제와 동일).
	// 이전 인스턴스를 먼저 멈추고 새로 재생해 항상 한 번만 들리게 한다.
	if (IsValid(AlarmAudioComponent))
	{
		AlarmAudioComponent->Stop();
	}
	AlarmAudioComponent = UGameplayStatics::SpawnSoundAtLocation(World, AlarmSound, GetActorLocation(),
		FRotator::ZeroRotator, AlarmVolumeMultiplier);
}
