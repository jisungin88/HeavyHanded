#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CheckWorldAlert.generated.h"

// 월드 경계도가 임계값 이상일 때만 해당 브랜치(Pursue 등)를 허용하는 조건 노드.
// 매 틱 평가되므로 반드시 C++로 구현한다.
UCLASS()
class UBTDecorator_CheckWorldAlert : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckWorldAlert();

	UPROPERTY(EditAnywhere, Category = "Condition")
	float AlertThreshold = 67.f; // 예: 경계(Alerted) 단계 이상

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
