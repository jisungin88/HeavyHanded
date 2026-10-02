#include "UI/DetectionGaugeWidget.h"
#include "Components/ProgressBar.h"

FString UDetectionGaugeWidget::GetGaugeBindingDebugInfo() const
{
	const FVector2D LayoutSize = GetCachedGeometry().GetLocalSize();
	const FVector2D SightSize = IsValid(SightGaugeBar) ? SightGaugeBar->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	const FVector2D HearingSize = IsValid(HearingGaugeBar) ? HearingGaugeBar->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	return FString::Printf(TEXT("시야바=%s 청각바=%s 위젯표시=%s 요청크기=%.0fx%.0f 실제배치=%.0fx%.0f 투명도=%.2f 배율=(%.2f,%.2f) 시야바배치=%.0fx%.0f 청각바배치=%.0fx%.0f"), IsValid(SightGaugeBar) ? TEXT("연결") : TEXT("없음(이름 확인)"), IsValid(HearingGaugeBar) ? TEXT("연결") : TEXT("없음(이름 확인)"), IsVisible() ? TEXT("켜짐") : TEXT("숨김"), GetDesiredSize().X, GetDesiredSize().Y, LayoutSize.X, LayoutSize.Y, GetRenderOpacity(), GetRenderTransform().Scale.X, GetRenderTransform().Scale.Y, SightSize.X, SightSize.Y, HearingSize.X, HearingSize.Y);
}

void UDetectionGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 소유자(AGuardCharacter)가 첫 게이지 갱신을 호출하기 전까지, 위젯이
	// 디자이너에서 설정한 기본 Visibility(보통 Visible)로 잠깐 노출되는 플리커를 막는다.
	// bHideWhenEmpty가 꺼져 있으면(=항상 표시 의도) 여기서 숨기면 안 된다 - 그 경우
	// 첫 갱신 전에도 디자이너의 표시 설정을 유지한다.
	if (bHideWhenEmpty)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UDetectionGaugeWidget::SetGaugePercent(float InPercent0to100)
{
	SetPerceptionGaugePercents(InPercent0to100, 0.f, true, false);
}

void UDetectionGaugeWidget::SetPerceptionGaugePercents(float SightPercent, float HearingPercent, bool bSightEnabled, bool bHearingEnabled)
{
	const float Sight01 = bSightEnabled ? FMath::Clamp(SightPercent / 100.f, 0.f, 1.f) : 0.f;
	const float Hearing01 = bHearingEnabled ? FMath::Clamp(HearingPercent / 100.f, 0.f, 1.f) : 0.f;
	if (IsValid(SightGaugeBar))
	{
		SightGaugeBar->SetPercent(Sight01);
		SightGaugeBar->SetVisibility(bSightEnabled && (!bHideWhenEmpty || Sight01 > 0.f) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (IsValid(HearingGaugeBar))
	{
		HearingGaugeBar->SetPercent(Hearing01);
		HearingGaugeBar->SetVisibility(bHearingEnabled && (!bHideWhenEmpty || Hearing01 > 0.f) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// Hidden으로 슬롯 간격을 유지해 한쪽 게이지가 사라져도 다른 쪽의 위치가 변하지 않는다.
	const bool bShouldShow = (bSightEnabled || bHearingEnabled) && (!bHideWhenEmpty || Sight01 > 0.f || Hearing01 > 0.f);
	SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
