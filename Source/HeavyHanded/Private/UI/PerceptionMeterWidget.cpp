#include "UI/PerceptionMeterWidget.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

#include "Noise/PerceptionMeterComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeavyUI, Log, All);

void UPerceptionMeterWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	UPerceptionMeterComponent* Meter = Guard->FindComponentByClass<UPerceptionMeterComponent>();
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
}

void UPerceptionMeterWidget::HandlePerceptionChanged(float NewPerception01)
{
	OnPerceptionUpdated(NewPerception01);

	// 게이지가 차오르는 동안이 플레이어의 유예 시간이다. 0 일 때는 띄우지 않는다
	const bool bShouldShow = NewPerception01 > KINDA_SMALL_NUMBER;
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
