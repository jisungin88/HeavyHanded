#include "UI/ResultRowWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "UI/UISettings.h"

void UResultRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// PreConstruct 는 디자이너에서도 실행된다. 토큰을 여기서 적용해야 편집 중에도
	// 실제 색 · 폰트가 보이고, UMG 에 색이 구워지는 것을 막는다 (UISettings.h 주석)
	if (Txt_Label)
	{
		Txt_Label->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Label->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_Amount)
	{
		Txt_Amount->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Amount->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::Money)));
	}

	if (Bar_Share)
	{
		// 기여도는 '많이 벌어왔다' 는 뜻이라 금액과 같은 색으로 묶는다
		Bar_Share->SetFillColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::Money));
	}
}

void UResultRowWidget::SetRow(const FText& Label, const FText& Amount, float Ratio, bool bDimmed)
{
	if (Txt_Label)
	{
		Txt_Label->SetText(Label);
	}

	if (Txt_Amount)
	{
		Txt_Amount->SetText(Amount);
	}

	if (Bar_Share)
	{
		// 음수는 '이 목록에는 바가 없다' 는 뜻이다. 0 과 구분해야 한다 —
		// 0 은 "기여가 없다" 라서 빈 바를 그려야 맞고, 음수는 바 자체가 없어야 한다
		if (Ratio < 0.f)
		{
			Bar_Share->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Bar_Share->SetVisibility(ESlateVisibility::HitTestInvisible);
			Bar_Share->SetPercent(FMath::Clamp(Ratio, 0.f, 1.f));
		}
	}

	// 색을 바꾸지 않고 행 전체의 알파만 낮춘다 (DimmedOpacity 주석)
	SetRenderOpacity(bDimmed ? DimmedOpacity : 1.f);

	OnRowSet(bDimmed);
}
