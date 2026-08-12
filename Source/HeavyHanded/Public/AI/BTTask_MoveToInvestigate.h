#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MoveToInvestigate.generated.h"

// InvestigateLocation(소음/의심 지점)으로 이동한다.
// 도착 후 SearchStartTime을 기록해, 이후 Decorator가 수색 타임아웃을 판정하도록 한다.
UCLASS()
class UBTTask_MoveToInvestigate : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MoveToInvestigate();

	UPROPERTY(EditAnywhere, Category = "Investigate")
	float AcceptanceRadius = 60.f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
