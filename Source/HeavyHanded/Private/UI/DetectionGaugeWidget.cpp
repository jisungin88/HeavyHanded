#include "UI/DetectionGaugeWidget.h"
#include "Components/ProgressBar.h"

void UDetectionGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 소유자(AGuardAIController)가 첫 SetGaugePercent를 호출하기 전까지, 위젯이
	// 디자이너에서 설정한 기본 Visibility(보통 Visible)로 잠깐 노출되는 플리커를 막는다.
	// bHideWhenEmpty가 꺼져 있으면(=항상 표시 의도) 여기서 숨기면 안 된다 - 그 경우
	// SetGaugePercent도 가시성을 건드리지 않으므로 아무도 다시 켜주지 않는다.
	if (bHideWhenEmpty)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UDetectionGaugeWidget::SetGaugePercent(float InPercent0to100)
{
	if (GaugeBar)
	{
		GaugeBar->SetPercent(InPercent0to100 / 100.f);
	}

	if (bHideWhenEmpty)
	{
		// Collapsed 대신 Hidden을 쓴다 - Collapsed는 레이아웃에서 완전히 빠지면서
		// Slate의 Auto Tick 판정에서도 제외될 수 있는데, 이 위젯은 외부에서 계속
		// SetGaugePercent를 불러줘야 다시 켜지므로 레이아웃 자체는 유지하는 Hidden이 안전하다.
		SetVisibility(InPercent0to100 > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
