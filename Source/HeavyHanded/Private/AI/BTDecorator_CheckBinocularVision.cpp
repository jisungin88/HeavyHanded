#include "AI/BTDecorator_CheckBinocularVision.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"
#include "AI/GuardSightAComponent.h"

UBTDecorator_CheckBinocularVision::UBTDecorator_CheckBinocularVision()
{
	NodeName = TEXT("Check Binocular Vision");
}

bool UBTDecorator_CheckBinocularVision::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!IsValid(AIController) || !IsValid(BlackboardComp))
	{
		return false;
	}

	AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));
	if (!IsValid(Target))
	{
		return false;
	}

	const UGuardSightAComponent* GuardSightComp = AIController->FindComponentByClass<UGuardSightAComponent>();
	if (!IsValid(GuardSightComp))
	{
		return false;
	}

	return IsValid(Target);
	//return GuardSightComp->GetBinocularVisionRate(Target) > 0.f;
}
