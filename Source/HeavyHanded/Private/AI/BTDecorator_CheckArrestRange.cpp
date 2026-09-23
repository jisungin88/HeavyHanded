#include "AI/BTDecorator_CheckArrestRange.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"

UBTDecorator_CheckArrestRange::UBTDecorator_CheckArrestRange()
{
	NodeName = TEXT("Check Arrest Range");
}

bool UBTDecorator_CheckArrestRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!IsValid(AIController) || !IsValid(BlackboardComp))
	{
		return false;
	}

	const APawn* GuardPawn = AIController->GetPawn();
	const AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));

	if (!IsValid(GuardPawn) || !IsValid(TargetActor))
	{
		return false;
	}

	const float Distance = FVector::Dist(GuardPawn->GetActorLocation(), TargetActor->GetActorLocation());

	return Distance <= ArrestRange;
}
