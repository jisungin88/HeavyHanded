#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DetectionGaugeWidget.generated.h"

class UProgressBar;

// 경비 머리 위에 붙는 시야 인지 게이지 표시 전용 위젯. 값을 스스로 찾아오지 않고
// SetGaugePercent()로 외부(AGuardAIController)가 떠먹여주는 값만 그대로 그린다 -
// "이 게이지가 누구 것인지"는 이 위젯을 소유한 WidgetComponent가 어느 경비 캐릭터에
// 붙어 있는지로 이미 결정되기 때문에, 위젯 스스로 월드를 순회해 대상을 찾을 필요가 없다.
//
// GaugeBar 는 파생 WBP(예: WBP_DetectionGauge)에 같은 이름의 ProgressBar를 배치해야
// 자동으로 연결된다(BindWidgetOptional이라 없어도 크래시하지 않고 그냥 갱신만 안 된다).
UCLASS()
class HEAVYHANDED_API UDetectionGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 0~100 퍼센트 값을 받아 바를 채우고, bHideWhenEmpty면 가시성도 같이 판정한다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Perception")
	void SetGaugePercent(float InPercent0to100);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Guard|Perception", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> GaugeBar;

	// 게이지가 0(시야에 없음)일 때 위젯을 통째로 숨길지.
	UPROPERTY(EditAnywhere, Category = "Guard|Perception")
	bool bHideWhenEmpty = true;
};
