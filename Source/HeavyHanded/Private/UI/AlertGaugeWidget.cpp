#include "UI/AlertGaugeWidget.h"

#include "Engine/World.h"
#include "TimerManager.h"

#include "Alert/AlertComponent.h"
#include "UI/UISettings.h"

void UAlertGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UAlertGaugeWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
	}

	// 구독을 안 풀면 위젯이 사라진 뒤에도 델리게이트에 남는다
	if (UAlertComponent* Alert = BoundAlert.Get())
	{
		Alert->OnAlertGaugeChanged.RemoveDynamic(this, &UAlertGaugeWidget::HandleGaugeChanged);
		Alert->OnAlertLevelChanged.RemoveDynamic(this, &UAlertGaugeWidget::HandleLevelChanged);
	}
	BoundAlert = nullptr;

	Super::NativeDestruct();
}

void UAlertGaugeWidget::TryBind()
{
	if (BoundAlert.Get())
	{
		return;
	}

	UAlertComponent* Alert = UAlertComponent::Get(this);
	if (!Alert)
	{
		// 클라이언트에서는 GameState 와 경계도 컴포넌트가 복제로 뒤늦게 도착한다
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
					BindRetryHandle, this, &UAlertGaugeWidget::TryBind, BindRetryInterval, false);
		}
		return;
	}

	BoundAlert = Alert;

	Alert->OnAlertGaugeChanged.AddDynamic(this, &UAlertGaugeWidget::HandleGaugeChanged);
	Alert->OnAlertLevelChanged.AddDynamic(this, &UAlertGaugeWidget::HandleLevelChanged);

	// 구독 시점의 값으로 한 번 그린다. 안 하면 다음 소음이 날 때까지 빈 게이지가 보인다
	const EAlertLevel Level = Alert->GetAlertLevel();
	HandleGaugeChanged(Alert->GetAlertGauge01());
	HandleLevelChanged(Level, Level);
}

void UAlertGaugeWidget::HandleGaugeChanged(float NewGauge01)
{
	OnGaugeUpdated(NewGauge01);
}

void UAlertGaugeWidget::HandleLevelChanged(EAlertLevel NewLevel, EAlertLevel OldLevel)
{
	OnLevelUpdated(NewLevel, OldLevel, UUISettings::Get()->GetAlertLevelColor(NewLevel));
}

// ──────────────────────────────────────────────────────────────
// 조회
// ──────────────────────────────────────────────────────────────

float UAlertGaugeWidget::GetGauge01() const
{
	const UAlertComponent* Alert = BoundAlert.Get();
	return Alert ? Alert->GetAlertGauge01() : 0.f;
}

EAlertLevel UAlertGaugeWidget::GetAlertLevel() const
{
	const UAlertComponent* Alert = BoundAlert.Get();
	return Alert ? Alert->GetAlertLevel() : EAlertLevel::Calm;
}

FLinearColor UAlertGaugeWidget::GetLevelColor() const
{
	return UUISettings::Get()->GetAlertLevelColor(GetAlertLevel());
}

FText UAlertGaugeWidget::GetLevelText() const
{
	return UUISettings::GetAlertLevelText(GetAlertLevel());
}

bool UAlertGaugeWidget::IsAlarmed() const
{
	const UAlertComponent* Alert = BoundAlert.Get();
	return Alert && Alert->IsAlarmed();
}
