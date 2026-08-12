#include "AI/BTTask_SelectNextPatrolPoint.h"
#include "AI/GuardAIController.h"
#include "AI/GuardTypes.h"

UBTTask_SelectNextPatrolPoint::UBTTask_SelectNextPatrolPoint()
{
	NodeName = TEXT("Select Next Patrol Point");
}

EBTNodeResult::Type UBTTask_SelectNextPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(GuardController))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("AGuardAIController 가 아니라 순찰 지점을 고를 수 없다 (현재 컨트롤러: %s). "
				 "폰의 AI Controller Class 를 확인할 것."),
			*GetNameSafe(OwnerComp.GetAIOwner()));
		return EBTNodeResult::Failed;
	}

	GuardController->SelectNextPatrolPoint();
	return EBTNodeResult::Succeeded;
}
