#include "AI/BTTask_SelectSearchPoint.h"
#include "AI/GuardAIController.h"
#include "AI/GuardTypes.h"

UBTTask_SelectSearchPoint::UBTTask_SelectSearchPoint()
{
	NodeName = TEXT("Select Search Point");
}

EBTNodeResult::Type UBTTask_SelectSearchPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(GuardController))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("AGuardAIController 가 아니라 수색 지점을 고를 수 없다 (현재 컨트롤러: %s)."),
			*GetNameSafe(OwnerComp.GetAIOwner()));
		return EBTNodeResult::Failed;
	}

	// false = 이번 조사에서 훑을 지점을 다 소진했다. Failed 로 브랜치를 끝내
	// Selector 가 순찰로 내려가게 한다.
	return GuardController->SelectNextSearchPoint()
		? EBTNodeResult::Succeeded
		: EBTNodeResult::Failed;
}
