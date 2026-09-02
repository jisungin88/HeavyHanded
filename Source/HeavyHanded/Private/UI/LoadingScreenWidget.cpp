#include "UI/LoadingScreenWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/UISettings.h"

void ULoadingScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyTokens();

	// 디자이너에는 아직 게임에서 온 문구가 없다. 미리보기로 자리를 채워야
	// 글자 크기와 줄 간격을 눈으로 맞출 수 있다
	ApplyContentTo(IsDesignTime() ? PreviewContent : Content);
}

void ULoadingScreenWidget::ApplyContent(const FLoadingScreenContent& NewContent)
{
	Content = NewContent;
	ApplyContentTo(Content);
}

void ULoadingScreenWidget::SetStatusText(const FText& NewStatus)
{
	Content.Status = NewStatus;

	if (Txt_Status)
	{
		Txt_Status->SetText(NewStatus);
	}
}

void ULoadingScreenWidget::ApplyTokens()
{
	if (Img_Background)
	{
		Img_Background->SetColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::BgBase));
	}

	if (Txt_Eyebrow)
	{
		Txt_Eyebrow->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
		Txt_Eyebrow->SetColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::TextSecondary));
	}

	if (Txt_Title)
	{
		Txt_Title->SetFont(UUISettings::GetUIFont(EUIFontToken::Title));
		Txt_Title->SetColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::TextPrimary));
	}

	if (Txt_TipLabel)
	{
		Txt_TipLabel->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
		Txt_TipLabel->SetColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::Gold));
	}

	if (Txt_Tip)
	{
		Txt_Tip->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Tip->SetColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::TextPrimary));
	}

	if (Txt_Status)
	{
		Txt_Status->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
		Txt_Status->SetColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::TextSecondary));
	}
}

void ULoadingScreenWidget::ApplyContentTo(const FLoadingScreenContent& Source)
{
	if (Txt_Eyebrow)
	{
		Txt_Eyebrow->SetText(Source.Eyebrow);
	}

	if (Txt_Title)
	{
		Txt_Title->SetText(Source.Title);
	}

	// 팁이 없으면 "TIP" 머리말만 덩그러니 남는다. 머리말과 본문을 같이 숨긴다
	const bool bHasTip = !Source.Tip.IsEmpty();
	const ESlateVisibility TipVisibility = bHasTip ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (Txt_TipLabel)
	{
		Txt_TipLabel->SetText(Source.TipLabel);
		Txt_TipLabel->SetVisibility(TipVisibility);
	}

	if (Txt_Tip)
	{
		Txt_Tip->SetText(Source.Tip);
		Txt_Tip->SetVisibility(TipVisibility);
	}

	if (Txt_Status)
	{
		Txt_Status->SetText(Source.Status);
	}
}
