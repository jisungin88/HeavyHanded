#include "AI/BTDecorator_CheckSearchTimeout.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTDecorator_CheckSearchTimeout::UBTDecorator_CheckSearchTimeout()
{
	NodeName = TEXT("Check Search Timeout");
}

bool UBTDecorator_CheckSearchTimeout::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!IsValid(AIController) || !IsValid(BlackboardComp))
	{
		return false;
	}

	const float SearchStartTime = BlackboardComp->GetValueAsFloat(TimeKeyName);
	const float Elapsed = AIController->GetWorld()->GetTimeSeconds() - SearchStartTime;

	return Elapsed < TimeoutSeconds;
}
