#include "UI/PerceptionMeterWidget.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "Noise/PerceptionMeterComponent.h"
#include "UI/HeavyUILog.h"
#include "Components/ProgressBar.h"
#include "AIController.h"

// UI 카테고리의 유일한 정의. 선언은 UI/HeavyUILog.h 에 있다 —
// STATIC 으로 두면 위젯 .cpp 가 둘 이상일 때 unity build 에서 재정의로 깨진다
DEFINE_LOG_CATEGORY(LogHeavyUI);

void UPerceptionMeterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogHeavyUI, Warning,
		TEXT("PerceptionMeterWidget Construct - GaugeBar: %s"), GaugeBar ? TEXT("VALID") : TEXT("NULL"));

	if (GaugeBar)
	{
		GaugeBar->SetPercent(0.f);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
				UnboundWarnHandle, this, &UPerceptionMeterWidget::WarnIfUnbound, UnboundWarnDelay, false);
	}
}

void UPerceptionMeterWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UnboundWarnHandle);
	}

	Unbind();

	Super::NativeDestruct();
}

void UPerceptionMeterWidget::BindToGuard(AActor* Guard)
{
	Unbind();

	if (!IsValid(Guard))
	{
		return;
	}


	APawn* GuardPawn = Cast<APawn>(Guard);
	if (!GuardPawn)
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("%s 가 Pawn이 아닙니다."), *Guard->GetName());
		return;
	}

	AAIController* AIController = Cast<AAIController>(GuardPawn->GetController());
	if (!AIController)
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("%s 의 AIController를 찾지 못했습니다."), *Guard->GetName());
		return;
	}


	UE_LOG(LogHeavyUI, Warning, TEXT("BindToGuard: AIController = %s"), *AIController->GetName());

	UPerceptionMeterComponent* Meter = AIController->FindComponentByClass<UPerceptionMeterComponent>();
	//UPerceptionMeterComponent* Meter = Guard->FindComponentByClass<UPerceptionMeterComponent>();

	if (!Meter)
	{
		UE_LOG(LogHeavyUI, Warning,
				TEXT("%s 에 UPerceptionMeterComponent 가 없어 인지 게이지가 동작하지 않습니다."),
				*Guard->GetName());
		return;
	}

	BoundMeter = Meter;
	Meter->OnPerceptionChanged.AddDynamic(this, &UPerceptionMeterWidget::HandlePerceptionChanged);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UnboundWarnHandle);
	}

	// 구독 시점의 값으로 한 번 그린다
	HandlePerceptionChanged(Meter->GetPerception01());
}

void UPerceptionMeterWidget::Unbind()
{
	if (UPerceptionMeterComponent* Meter = BoundMeter.Get())
	{
		// 구독을 안 풀면 위젯이 사라진 뒤에도 델리게이트에 남는다
		Meter->OnPerceptionChanged.RemoveDynamic(this, &UPerceptionMeterWidget::HandlePerceptionChanged);
	}
	BoundMeter = nullptr;
	bShown = false; //추가
}

void UPerceptionMeterWidget::UpdatePerceptionGauge(float NewPerception01)
{

}

void UPerceptionMeterWidget::HandlePerceptionChanged(float NewPerception01)
{
	const float Perception01 = FMath::Clamp(NewPerception01, 0.f, 1.f); // 0~1 clamp 방어

	UE_LOG(LogHeavyUI, Warning,
		TEXT("Perception Changed: %.2f / GaugeBar: %s"), Perception01, GaugeBar ? TEXT("VALID") : TEXT("NULL"));

	// 작동시 삭제
	//OnPerceptionUpdated(Perception01);
	if (GaugeBar)
	{
		GaugeBar->SetPercent(Perception01);
	}

	// 게이지가 차오르는 동안이 플레이어의 유예 시간이다. 0 일 때는 띄우지 않는다
	const bool bShouldShow = Perception01 > KINDA_SMALL_NUMBER;
	if (bShouldShow != bShown)
	{
		bShown = bShouldShow;
		OnMeterVisibilityChanged(bShouldShow);
	}
}

void UPerceptionMeterWidget::WarnIfUnbound()
{
	if (BoundMeter.Get())
	{
		return;
	}

	UE_LOG(LogHeavyUI, Warning,
			TEXT("%s 가 경비에 바인딩되지 않았습니다. 경비 BP 의 BeginPlay 에서 BindToGuard(self) 를 호출하세요."),
			*GetName());
}

// ──────────────────────────────────────────────────────────────
// 조회
// ──────────────────────────────────────────────────────────────

float UPerceptionMeterWidget::GetPerception01() const
{
	const UPerceptionMeterComponent* Meter = BoundMeter.Get();
	return Meter ? Meter->GetPerception01() : 0.f;
}

bool UPerceptionMeterWidget::IsLatched() const
{
	const UPerceptionMeterComponent* Meter = BoundMeter.Get();
	return Meter && Meter->IsLatched();
}
