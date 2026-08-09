#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "Noise/NoiseTypes.h"          // EAlertLevel — UFUNCTION 파라미터라 전방 선언 불가
#include "UISettings.generated.h"

// 색 토큰 (UISystem.md 2장). 시안 5장에서 픽셀 실측한 값이다.
// 이름은 토큰 표를 그대로 따른다 — 쓰는 자리(예: ButtonColor)로 이름을 붙이면
// 같은 색에 이름이 여러 개 생긴다
UENUM(BlueprintType)
enum class EUIColorToken : uint8
{
	BgBase         UMETA(DisplayName = "배경"),
	BgPanel        UMETA(DisplayName = "패널"),
	BgCard         UMETA(DisplayName = "카드"),
	Divider        UMETA(DisplayName = "구분선"),
	Gold           UMETA(DisplayName = "골드"),
	GoldBright     UMETA(DisplayName = "밝은 골드"),
	TextPrimary    UMETA(DisplayName = "본문"),
	TextSecondary  UMETA(DisplayName = "보조 텍스트"),
	Health         UMETA(DisplayName = "체력"),
	Money          UMETA(DisplayName = "금액")
};

// 폰트 3단계 (UISystem.md 2장)
UENUM(BlueprintType)
enum class EUIFontToken : uint8
{
	Timer  UMETA(DisplayName = "타이머"),      // 28 Bold — 남은 시간, 목표 금액
	Value  UMETA(DisplayName = "수치"),        // 18 — 체력 %, 가격, 노획물 가치
	Label  UMETA(DisplayName = "라벨")         // 12 — 라벨, 부제
};

/**
 * 디자인 토큰 (UISystem.md 2장). Project Settings → Game → UI.
 *
 * 색과 폰트를 위젯마다 찍으면 HUD · 결과 화면 · 상점이 서로 어긋난다. 전부 이 한 곳을 본다.
 *
 * 색 에셋을 Content/ 에 두지 않은 이유: UMG 디자이너에 찍은 색은 그대로 구워져서
 * 토큰을 바꿔도 이미 만든 위젯이 따라오지 않는다. 위젯은 PreConstruct 에서 여기 값을 읽어
 * 자기 색을 칠한다 — PreConstruct 는 디자이너에서도 돌기 때문에 편집 중에도 그대로 보인다.
 *
 * UNoiseSettings / UAlertSettings 와 같은 패턴이다.
 */
UCLASS(config = UI, defaultconfig, BlueprintType, meta = (DisplayName = "UI"))
class HEAVYHANDED_API UUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUISettings();

	static const UUISettings* Get() { return GetDefault<UUISettings>(); }

	/** BP 용 접근자. UFUNCTION 반환에는 const 포인터를 쓰지 않는다 */
	UFUNCTION(BlueprintPure, Category = "UI")
	static UUISettings* GetUISettings() { return GetMutableDefault<UUISettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 * 토큰 색. 위젯 PreConstruct 에서 부른다.
	 *
	 * static 이라 BP 에서 노드 하나로 끝난다 (Get UI Settings 를 먼저 물지 않아도 된다)
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Palette")
	static FLinearColor GetUIColor(EUIColorToken Token);

	/**
	 * 토큰 폰트. 아래 프로퍼티가 비어 있으면 엔진 기본 폰트에 크기·굵기만 얹어 돌려준다.
	 *
	 * 폰트 프로퍼티를 직접 읽지 말 것 — 비어 있는 채로 위젯에 꽂으면 Slate 가 LastResort 폰트로
	 * 떨어져 글자가 전부 네모가 된다. 그 방어가 여기 있다
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Typography")
	static FSlateFontInfo GetUIFont(EUIFontToken Token);

	/** 경계 단계에 대응하는 게이지 색 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	FLinearColor GetAlertLevelColor(EAlertLevel Level) const;

	/** 단계 표시용 이름. EAlertLevel 의 UMETA(DisplayName) 을 그대로 쓴다 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	static FText GetAlertLevelText(EAlertLevel Level);

	// ── 색 토큰 (UISystem.md 2장) ──
	//
	// 시안이 압축 스크린샷이라 실측값은 근사다. 골드만 4장에서 hue 41 로 일치해 확실하다.
	// 시안 원본이 오면 여기 값만 고치면 전 화면이 따라온다

	/** 화면 바탕 — #14181F. 시안 5장 전체 최빈색 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BgBaseColor = FLinearColor::FromSRGBColor(FColor(0x14, 0x18, 0x1F));

	/** 패널 바탕 — #1A1E28 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BgPanelColor = FLinearColor::FromSRGBColor(FColor(0x1A, 0x1E, 0x28));

	/** 카드 바탕 — #20242E. 상점 장비 카드 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BgCardColor = FLinearColor::FromSRGBColor(FColor(0x20, 0x24, 0x2E));

	/** 구분선 — #2C3140. 시안에서 측정되지 않아 제안값이다 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor DividerColor = FLinearColor::FromSRGBColor(FColor(0x2C, 0x31, 0x40));

	/** 액센트 골드 — #DEA934. 선택 테두리, 주 버튼, 강조 수치 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor GoldColor = FLinearColor::FromSRGBColor(FColor(0xDE, 0xA9, 0x34));

	/** 밝은 골드 — #E9AF31. 타이틀 로고 전용 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor GoldBrightColor = FLinearColor::FromSRGBColor(FColor(0xE9, 0xAF, 0x31));

	/** 본문 텍스트 — #F2F4F8 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor TextPrimaryColor = FLinearColor::FromSRGBColor(FColor(0xF2, 0xF4, 0xF8));

	/** 라벨 · 부제 — #8A93A3. 경계도 평온과 같은 색이다 (의도된 것) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor TextSecondaryColor = FLinearColor::FromSRGBColor(FColor(0x8A, 0x93, 0xA3));

	/** 체력 — #80E080 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor HealthColor = FLinearColor::FromSRGBColor(FColor(0x80, 0xE0, 0x80));

	/** 가격 · 정산 금액 — #6FD08C */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor MoneyColor = FLinearColor::FromSRGBColor(FColor(0x6F, 0xD0, 0x8C));

	// ── 타이포그래피 ──
	//
	// 크기를 위젯마다 찍으면 화면별로 제각각이 되므로 세 단계 밖으로 나가지 않는다.
	//
	// Font 슬롯이 비어 있으면 엔진 기본 폰트를 쓴다 (게임 폰트 미정). 정해지면 여기 세 개만 꽂는다.
	// BP 노출은 하지 않는다 — 비어 있는 값을 그대로 위젯에 꽂으면 글자가 네모가 되므로
	// 창구는 GetUIFont() 하나뿐이다

	/** 타이머 · 목표 금액처럼 화면에서 제일 큰 수치 */
	UPROPERTY(config, EditAnywhere, Category = "Typography")
	FSlateFontInfo TimerFont;

	/** 일반 수치 — 체력 %, 가격, 노획물 가치 */
	UPROPERTY(config, EditAnywhere, Category = "Typography")
	FSlateFontInfo ValueFont;

	/** 라벨 · 부제 */
	UPROPERTY(config, EditAnywhere, Category = "Typography")
	FSlateFontInfo LabelFont;

	// ── 경계도 4단계 색상 (기획서 8장 "경계도 게이지(4단계 색상)") ──
	//
	// 시안에 경계도 게이지 자체가 없어서 새로 잡은 값이다.
	// 체력 초록(#80E080)과 겹치지 않도록 평온을 무채로 두고,
	// 의심에 시안 전체의 액센트 골드(#DEA934)를 재사용해 팔레트 안에서 상승감을 만든다.
	//
	// 색만으로 단계를 구분하지 않는다. 위젯은 GetAlertLevelText() 를 항상 같이 띄운다.

	/** 평온 — #8A93A3 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Alert Gauge")
	FLinearColor CalmColor = FLinearColor::FromSRGBColor(FColor(0x8A, 0x93, 0xA3));

	/** 의심 — #DEA934 (시안 액센트 골드) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Alert Gauge")
	FLinearColor SuspiciousColor = FLinearColor::FromSRGBColor(FColor(0xDE, 0xA9, 0x34));

	/** 경계 — #E3762F */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Alert Gauge")
	FLinearColor AlertedColor = FLinearColor::FromSRGBColor(FColor(0xE3, 0x76, 0x2F));

	/** 경보 — #D93A34 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Alert Gauge")
	FLinearColor AlarmColor = FLinearColor::FromSRGBColor(FColor(0xD9, 0x3A, 0x34));

	/**
	 * 경보 단계 점멸 속도. 0 이면 점멸하지 않는다.
	 *
	 * WBP_AlertGauge 의 AlarmBlink 애니메이션은 2Hz 로 작성돼 있다.
	 * Play Animation 의 Playback Speed 에 AlarmBlinkHz / 2 를 넣어야 이 값이 반영된다
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Alert Gauge", meta = (ClampMin = "0.0", Units = "Hz"))
	float AlarmBlinkHz = 2.f;
};
