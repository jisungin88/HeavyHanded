#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SelectNextPatrolPoint.generated.h"

// AGuardAIController::SelectNextPatrolPoint()를 호출해 Blackboard의 PatrolLocation을 갱신한다.
// Patrol 브랜치에서 Move To(PatrolLocation) 이전에 배치한다.
UCLASS()
class UBTTask_SelectNextPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SelectNextPatrolPoint();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
