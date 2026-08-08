#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Noise/NoiseTypes.h"          // EAlertLevel — UFUNCTION 파라미터라 전방 선언 불가
#include "UISettings.generated.h"

/**
 * UI 디자인 토큰 중 C++ 이 읽는 것들. Project Settings → Game → UI.
 *
 * 레이아웃 · 폰트 · 브러시는 에디터에서 UMG 스타일 에셋으로 잡는다.
 * 여기에는 코드가 계산에 쓰는 값만 둔다 — 지금은 경계도 4단계 색상뿐이다.
 *
 * 색을 위젯마다 하드코딩하면 HUD · 결과 화면 · 미니맵의 4단계가 서로 어긋난다.
 * 전부 이 한 곳을 본다.
 *
 * UNoiseSettings / UAlertSettings 와 같은 패턴이다.
 */
UCLASS(config = UI, defaultconfig, BlueprintType, meta = (DisplayName = "UI"))
class HEAVYHANDED_API UUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UUISettings* Get() { return GetDefault<UUISettings>(); }

	/** BP 용 접근자. UFUNCTION 반환에는 const 포인터를 쓰지 않는다 */
	UFUNCTION(BlueprintPure, Category = "UI")
	static UUISettings* GetUISettings() { return GetMutableDefault<UUISettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 경계 단계에 대응하는 게이지 색 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	FLinearColor GetAlertLevelColor(EAlertLevel Level) const;

	/** 단계 표시용 이름. EAlertLevel 의 UMETA(DisplayName) 을 그대로 쓴다 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	static FText GetAlertLevelText(EAlertLevel Level);

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
