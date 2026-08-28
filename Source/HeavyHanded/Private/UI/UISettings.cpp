#include "UI/UISettings.h"

#include "Styling/CoreStyle.h"
#include "UObject/Class.h"
#include "UObject/ReflectedTypeAccessors.h"

UUISettings::UUISettings()
{
	// 폰트 오브젝트는 생성자에서 잡지 않는다.
	//
	// UTextBlock 처럼 ConstructorHelpers 로 /Engine/EngineFonts/Roboto 를 찾으면 게임 모듈에서는
	// 조용히 실패한다 — 이 CDO 는 모듈 로드 중에 만들어지고 그 시점에 엔진 폰트를 못 읽는다.
	// FontObject 가 null 인 FSlateFontInfo 는 컴파일도 경고도 통과하지만, Slate 가 LastResort
	// 폰트로 떨어져 한글이든 숫자든 글자가 전부 네모로 나온다 (FSlateFontInfo::GetCompositeFont).
	//
	// 여기서는 크기와 굵기만 정하고, 실제 폰트는 GetUIFont() 가 FCoreStyle 에서 가져온다.
	// FCoreStyle 의 기본 폰트는 에셋 로드가 필요 없어 언제 불러도 안전하고 한글 폴백도 들어 있다
	TimerFont.Size = 28.f;
	TimerFont.TypefaceFontName = TEXT("Bold");

	ValueFont.Size = 18.f;
	ValueFont.TypefaceFontName = TEXT("Regular");

	LabelFont.Size = 12.f;
	LabelFont.TypefaceFontName = TEXT("Regular");
}

FSlateFontInfo UUISettings::GetUIFont(EUIFontToken Token)
{
	const UUISettings* Settings = Get();

	const FSlateFontInfo& Configured = (Token == EUIFontToken::Timer) ? Settings->TimerFont
		: (Token == EUIFontToken::Value) ? Settings->ValueFont
		: Settings->LabelFont;

	// Project Settings 에서 게임 폰트를 꽂았으면 그대로 쓴다
	if (Configured.HasValidFont())
	{
		return Configured;
	}

	const FName Typeface = Configured.TypefaceFontName.IsNone() ? FName(TEXT("Regular")) : Configured.TypefaceFontName;
	return FCoreStyle::GetDefaultFontStyle(Typeface, Configured.Size);
}

FLinearColor UUISettings::GetUIColor(EUIColorToken Token)
{
	const UUISettings* Settings = Get();

	switch (Token)
	{
	case EUIColorToken::BgBase:        return Settings->BgBaseColor;
	case EUIColorToken::BgPanel:       return Settings->BgPanelColor;
	case EUIColorToken::BgCard:        return Settings->BgCardColor;
	case EUIColorToken::Divider:       return Settings->DividerColor;
	case EUIColorToken::Gold:          return Settings->GoldColor;
	case EUIColorToken::GoldBright:    return Settings->GoldBrightColor;
	case EUIColorToken::TextPrimary:   return Settings->TextPrimaryColor;
	case EUIColorToken::TextSecondary: return Settings->TextSecondaryColor;
	case EUIColorToken::Health:        return Settings->HealthColor;
	case EUIColorToken::Money:         return Settings->MoneyColor;
	}

	return Settings->TextPrimaryColor;
}

FLinearColor UUISettings::GetAlertLevelColor(EAlertLevel Level) const
{
	switch (Level)
	{
	case EAlertLevel::Calm:       return CalmColor;
	case EAlertLevel::Suspicious: return SuspiciousColor;
	case EAlertLevel::Alerted:    return AlertedColor;
	case EAlertLevel::Alarm:      return AlarmColor;
	}

	return CalmColor;
}

FText UUISettings::GetAlertLevelText(EAlertLevel Level)
{
	// EAlertLevel 의 UMETA(DisplayName) 이 이미 한글 이름을 갖고 있다.
	// 여기서 또 정의하면 enum 이 바뀔 때 조용히 어긋난다
	return StaticEnum<EAlertLevel>()->GetDisplayNameTextByValue(static_cast<int64>(Level));
}

TSoftObjectPtr<UTexture2D> UUISettings::GetHeldSlotIcon(const FGameplayTagContainer& TypeTags) const
{
	// 특성 태그는 여럿일 수 있다 — 대형 금고는 '중량형 + 경보 연동형' 처럼 겹친다(기획서 5장).
	// 표에 있는 첫 번째를 쓴다. 우선순위를 정해야 할 만큼 겹치기 시작하면 그때 배열로 바꾼다
	for (const FGameplayTag& Tag : TypeTags)
	{
		if (const TSoftObjectPtr<UTexture2D>* Found = HeldSlotIcons.Find(Tag))
		{
			// 표에 행은 있는데 그림을 안 꽂아 둔 경우가 있다. 그때는 없는 것으로 친다
			if (!Found->IsNull())
			{
				return *Found;
			}
		}
	}

	return HeldSlotFallbackIcon;
}
