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

	// 0~100 퍼센트. AGuardAIController::GetWorldAlertLevel() 이 UAlertComponent 의
	// 0~1 게이지를 퍼센트로 바꿔 돌려주므로 Alert.ini 의 단계 숫자를 그대로 쓰면 된다.
	//   Calm 0~33 / Suspicious 34~66 / Alerted 67~99 / Alarm 100
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float AlertThreshold = 67.f; // 경계(Alerted) 단계 이상

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
