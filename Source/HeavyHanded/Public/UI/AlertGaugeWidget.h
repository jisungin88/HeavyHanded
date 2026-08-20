#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Noise/NoiseTypes.h"          // EAlertLevel — UFUNCTION 파라미터라 전방 선언 불가
#include "AlertGaugeWidget.generated.h"

class UAlertComponent;

/**
 * 경계도 게이지 HUD (기획서 8장).
 *
 * UAlertComponent 는 GameState 에 런타임 부착되고 클라이언트에는 복제로 도착한다.
 * 위젯이 먼저 만들어질 수 있어서 붙을 때까지 재시도한다 — 한 번만 찾고 포기하면
 * 클라이언트에서만 게이지가 영원히 0 으로 남는다.
 *
 * 레이아웃 · 색 전환 · 점멸은 이 클래스를 상속한 WBP 에서 한다.
 * C++ 은 구독과 값 전달만 담당한다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UAlertGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 0~1. 아직 못 붙었으면 0 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	float GetGauge01() const;

	/**
	 * 히스테리시스가 있어서 단계는 게이지만으로 결정되지 않는다.
	 * 게이지 65% 가 상승 중이면 의심, 하강 중이면 경계다. 직접 계산하지 말 것
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	EAlertLevel GetAlertLevel() const;

	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	FLinearColor GetLevelColor() const;

	/** 색만으로 단계를 구분하지 않기 위한 표시용 이름 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	FText GetLevelText() const;

	/** 경보(래치) 상태인가. 90초 카운트다운 표시 조건 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	bool IsAlarmed() const;

protected:
	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	/** 게이지 값이 바뀌었다. 바 채우기와 보간을 여기서 한다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Alert")
	void OnGaugeUpdated(float NewGauge01);

	/**
	 * 단계가 바뀌었다. 색 전환 · 펄스 · 경보 점멸을 여기서 한다.
	 * 최초 바인딩 시에도 한 번 호출되며 이때는 NewLevel == OldLevel 이다
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Alert")
	void OnLevelUpdated(EAlertLevel NewLevel, EAlertLevel OldLevel, FLinearColor LevelColor);

private:
	UFUNCTION()
	void HandleGaugeChanged(float NewGauge01);

	UFUNCTION()
	void HandleLevelChanged(EAlertLevel NewLevel, EAlertLevel OldLevel);

	/** 성공할 때까지 재시도한다. 클라에서는 GameState 도 컴포넌트도 늦게 온다 */
	void TryBind();

	UPROPERTY()
	TObjectPtr<UAlertComponent> BoundAlert;

	FTimerHandle BindRetryHandle;

	/** 재바인딩 시도 간격 */
	static constexpr float BindRetryInterval = 0.25f;
};
