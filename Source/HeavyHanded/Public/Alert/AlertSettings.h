#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AlertSettings.generated.h"

/**
 * 경계도 밸런싱. Project Settings → Game → Alert.
 *
 * UAlertComponent 는 GameState 에 런타임 부착되기 때문에 디테일 패널에 뜨지 않는다.
 * 그래서 기획자가 만질 값은 컴포넌트가 아니라 전부 여기 둔다.
 * C++ / BP 어디에도 수치를 하드코딩하지 말 것 — 여기가 유일한 진리원이다.
 *
 * 소음 쪽 UNoiseSettings 와 같은 패턴이다.
 */
UCLASS(config = AlertSystem, defaultconfig, meta = (DisplayName = "Alert"))
class HEAVYHANDED_API UAlertSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** CDO 라 절대 null 이 아니다 — 호출부에서 null 검사를 하지 말 것. UNoiseSettings::Get() 주석 참고 */
	static const UAlertSettings* Get() { return GetDefault<UAlertSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ── 단계 임계값 (기획서 3장) ──
	//
	// 진입은 Enter, 이탈은 Exit 를 쓴다. 이 간격이 히스테리시스다.
	// 없으면 경계값 근처에서 단계가 깜빡이며 셔터가 열렸다 닫혔다 한다.

	/** 평온 → 의심 진입 */
	UPROPERTY(config, EditAnywhere, Category = "Threshold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SuspiciousEnter = 0.34f;

	/** 의심 → 평온 이탈. SuspiciousEnter 보다 낮아야 한다 */
	UPROPERTY(config, EditAnywhere, Category = "Threshold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SuspiciousExit = 0.30f;

	/** 의심 → 경계 진입 */
	UPROPERTY(config, EditAnywhere, Category = "Threshold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlertedEnter = 0.67f;

	/** 경계 → 의심 이탈. AlertedEnter 보다 낮아야 한다 */
	UPROPERTY(config, EditAnywhere, Category = "Threshold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlertedExit = 0.63f;

	// ── 자연 감소 ──

	/** 평온 · 의심 단계의 무소음 유예. 이 시간이 지나야 감소가 시작된다 */
	UPROPERTY(config, EditAnywhere, Category = "Decay", meta = (ClampMin = "0.0", Units = "s"))
	float SilenceSeconds = 30.f;

	/** 경계 단계의 무소음 유예. 경비가 더 오래 곤두서 있다 */
	UPROPERTY(config, EditAnywhere, Category = "Decay", meta = (ClampMin = "0.0", Units = "s"))
	float AlertedSilenceSeconds = 60.f;

	/** 초당 감소량. 0.01 == 1%/초 */
	UPROPERTY(config, EditAnywhere, Category = "Decay", meta = (ClampMin = "0.0"))
	float DecayPerSecond = 0.01f;

#if WITH_EDITOR
	/** 히스테리시스가 뒤집힌 값이 저장되지 않게 막는다 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
