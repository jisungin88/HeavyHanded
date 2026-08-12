#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BTDecorator_CheckDetectionGauge.generated.h"

// DetectionGauge가 임계값 이상인지 판정하는 조건 노드.
// AIState Blackboard 키를 쓰지 않고, BT 트리 구조(Selector 우선순위 + 이 Decorator)로
// Pursue / Investigate 분기를 직접 표현하기 위해 사용한다.
//
// UBTDecorator_BlackboardBase 를 상속하는 이유:
// 평범한 UBTDecorator 는 블랙보드를 구독하지 않아서, 에셋에서 Observer aborts 를
// 걸어둬도 스스로 재실행을 요청하지 못한다. 게이지가 차오르는 것을 계기로 하위
// 우선순위 브랜치(순찰·대기)를 끊으려면 이 클래스가 직접 옵저버여야 한다.
// 부모가 OnBecomeRelevant/OnCeaseRelevant 에서 등록·해제를 처리한다.
UCLASS()
class UBTDecorator_CheckDetectionGauge : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckDetectionGauge();

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float GaugeThreshold = 100.f;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
