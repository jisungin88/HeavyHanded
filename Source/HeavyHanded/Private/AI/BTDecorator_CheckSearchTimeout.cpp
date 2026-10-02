#include "AI/BTDecorator_CheckSearchTimeout.h"
#include "AIController.h"
#include "AI/GuardAIController.h"
#include "AI/GuardPatrolAComponent.h"
#include "AI/GuardBlackboardKeys.h"
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
	bool bTimeRemaining = Elapsed < TimeoutSeconds;
	if (AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner()))
	{
		if (TimeKeyName == GuardAIKeys::SearchStartTime && IsValid(GuardController->GuardPatrolComp))
		{
			GuardController->GuardPatrolComp->ConfigureSearchRetryPolicy(SearchStartTime, TimeoutSeconds, QuickRetryThresholdSeconds, QuickRetryExtensionSeconds);
			bTimeRemaining = GuardController->GuardPatrolComp->IsSearchTimeRemaining(SearchStartTime, TimeoutSeconds);
		}
		const FName Event(*FString::Printf(TEXT("Timeout_%s_%.2f_%d"), *TimeKeyName.ToString(), TimeoutSeconds, bTimeRemaining));
		GuardController->LogSearchTransitionDebug(Event, FString::Printf(TEXT("시간 조건 판정: Key=%s Age=%.3f Limit=%.3f RawPass=%d Inverted=%d"), *TimeKeyName.ToString(), Elapsed, TimeoutSeconds, bTimeRemaining, IsInversed()));
	}

	return bTimeRemaining;
}
