#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CheckDetectionGauge.generated.h"

// DetectionGauge가 임계값 이상인지 판정하는 조건 노드.
// AIState Blackboard 키를 쓰지 않고, BT 트리 구조(Selector 우선순위 + 이 Decorator)로
// Pursue / Investigate 분기를 직접 표현하기 위해 사용한다.
//
// 사용 예:
//   Pursue 브랜치    : 이 Decorator(Threshold=100) + 별도 Blackboard Decorator(CanSeeTarget == true)
//   Investigate 브랜치: 이 Decorator(Threshold=100) + 별도 Blackboard Decorator(CanSeeTarget == false)
UCLASS()
class UBTDecorator_CheckDetectionGauge : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckDetectionGauge();

	UPROPERTY(EditAnywhere, Category = "Condition")
	float GaugeThreshold = 100.f;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
