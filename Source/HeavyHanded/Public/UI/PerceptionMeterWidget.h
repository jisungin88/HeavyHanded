#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PerceptionMeterWidget.generated.h"

class AActor;
class UPerceptionMeterComponent;

/**
 * 경비 머리 위 인지 게이지 (기획서 8장). WidgetComponent 에 물려 월드 스페이스로 띄운다.
 *
 * 게이지가 차는 동안이 플레이어에게 주어지는 유예다 — 숨거나 멈출 시간.
 * 그래서 0 일 때는 숨기고, 차오르기 시작할 때만 보여야 눈에 띈다.
 *
 * **소유 경비는 스스로 알 수 없다.** UWidgetComponent::InitWidget() 이
 * CreateWidget(World, ...) 으로 만들기 때문에 Outer 가 World 이고 액터로 거슬러 올라갈 수 없다.
 * 경비 BP 에서 BindToGuard(self) 를 한 번 불러줘야 한다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UPerceptionMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이 게이지가 따라갈 경비를 지정한다. 경비 BP 의 BeginPlay 에서 한 번 호출한다.
	 *
	 *     WidgetComponent → Get User Widget Object → Cast To PerceptionMeterWidget
	 *                     → Bind To Guard (Self)
	 *
	 * 이미 다른 경비에 붙어 있었다면 그쪽 구독을 먼저 푼다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Perception")
	void BindToGuard(AActor* Guard);

	/** 0~1. 아직 못 붙었으면 0 */
	UFUNCTION(BlueprintPure, Category = "UI|Perception")
	float GetPerception01() const;

	/** 100% 도달 후 래치 상태인가. 조사 시작 연출 조건 */
	UFUNCTION(BlueprintPure, Category = "UI|Perception")
	bool IsLatched() const;

protected:
	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	/** 게이지 값이 바뀌었다. 원형 채우기와 보간을 여기서 한다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Perception")
	void OnPerceptionUpdated(float NewPerception01);

	/** 0 에서 벗어났거나 0 으로 돌아왔다. 페이드 인/아웃을 여기서 한다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Perception")
	void OnMeterVisibilityChanged(bool bShouldShow);

private:
	UFUNCTION()
	void HandlePerceptionChanged(float NewPerception01);

	void Unbind();

	/** 아무도 BindToGuard() 를 안 불렀을 때 조용히 실패하지 않도록 경고한다 */
	void WarnIfUnbound();

	UPROPERTY()
	TObjectPtr<UPerceptionMeterComponent> BoundMeter;

	FTimerHandle UnboundWarnHandle;

	/** 마지막으로 BP 에 알린 표시 여부. 매 틱 이벤트가 나가지 않게 막는다 */
	bool bShown = false;

	/** 이 시간까지 바인딩이 없으면 경고를 남긴다 */
	static constexpr float UnboundWarnDelay = 2.f;
};
