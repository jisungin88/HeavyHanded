#include "Hazards/LaserTrap.h"

#include "AbilitySystemComponent.h"
#include "Alert/AlertComponent.h"
#include "Character/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"   // FGameplayTag::RequestGameplayTag — State.ShadowStep 임시 문자열 조회
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

ALaserTrap::ALaserTrap()
{
	PrimaryActorTick.bCanEverTick = false;

	// Multicast_PlayAlarmSound 를 쓰려면 복제 액터여야 한다
	bReplicates = true;

	BeamEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BeamEffect"));
	SetRootComponent(BeamEffect);

	// 다른 Hazard 클래스들과 같은 이유로 폰 채널만 오버랩으로 연다.
	// 기본값은 얇고 넓게(레이저 통과 판정) — 실제 통로에 맞춰 BP 에서 조정할 것
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(BeamEffect);
	TriggerVolume->SetBoxExtent(FVector(5.f, 100.f, 50.f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ALaserTrap::OnTriggerOverlap);
}

void ALaserTrap::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "지나갔는데 아무 일도 안 일어난다" 하나로만 드러난다 (다른 Hazard 클래스와 동일 사유)
	if (!IsValid(BeamEffect) || !BeamEffect->GetAsset())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[LaserTrap:%s] BeamEffect 에 Niagara System 이 없다 — 눈에 안 보이는 레이저가 된다"),
			*GetName());
	}
}

void ALaserTrap::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
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

	// 그림자 이동 중엔 무시한다 — State.ini 의 State.ShadowStep 코멘트에 이미 "압력판 무시"
	// 라고 명시돼 있고, 레이저도 같은 종류의 감지 장치라 동일하게 적용한다
	// (APressurePlate 와 동일 사유 — 문자열 조회로 HeavyHandedGameplayTags.h 를 안 건드린다)
	if (UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent())
	{
		static const FGameplayTag ShadowStepTag = FGameplayTag::RequestGameplayTag(TEXT("State.ShadowStep"));
		if (ASC->HasMatchingGameplayTag(ShadowStepTag))
		{
			return;
		}
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (LastTriggerTime >= 0.f && Now - LastTriggerTime < MinRetriggerInterval)
	{
		// 오버랩 경계에서 스치듯 들락거린 것 — 진짜 재진입이 아니다 (다른 Hazard 클래스와 동일 사유)
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

	// 판정은 여기서 끝났다. 문을 닫는 등 레벨별 연결은 BP 몫이다 (헤더 주석 참고)
	OnLaserTriggered();

	UE_LOG(LogHazard, Log, TEXT("[LaserTrap:%s] %s 가 레이저를 가로질렀다 — 경보"),
		*GetName(), *Target->GetName());
}

void ALaserTrap::Multicast_PlayAlarmSound_Implementation()
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
