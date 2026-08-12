#include "AI/BTTask_SelectNextPatrolPoint.h"
#include "AI/GuardAIController.h"

UBTTask_SelectNextPatrolPoint::UBTTask_SelectNextPatrolPoint()
{
	NodeName = TEXT("Select Next Patrol Point");
}

EBTNodeResult::Type UBTTask_SelectNextPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(GuardController))
	{
		return EBTNodeResult::Failed;
	}

	GuardController->SelectNextPatrolPoint();
	return EBTNodeResult::Succeeded;
}
