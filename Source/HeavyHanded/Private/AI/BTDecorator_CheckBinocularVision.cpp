#include "AI/BTDecorator_CheckBinocularVision.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"
#include "AI/GuardSightAComponent.h"

UBTDecorator_CheckBinocularVision::UBTDecorator_CheckBinocularVision()
{
	NodeName = TEXT("Check Binocular Vision");
	BlackboardKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_CheckBinocularVision, BlackboardKey));
	BlackboardKey.SelectedKeyName = GuardAIKeys::CanSeeTarget;
}

FString UBTDecorator_CheckBinocularVision::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\n%s\nObserver Key: %s"), *Super::GetStaticDescription(), bAllowPeripheralObservation ? TEXT("Observe visible target (including peripheral vision)") : TEXT("Binocular vision only"), *BlackboardKey.SelectedKeyName.ToString());
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

	if (bAllowPeripheralObservation)
	{
		// 수색 시간이나 게이지 임계값과 독립적이다. 이동 실패 후에도 보이는 대상을 계속 바라볼 수 있다.
		return BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget);
	}

	const UGuardSightAComponent* GuardSightComp = AIController->FindComponentByClass<UGuardSightAComponent>();
	if (!IsValid(GuardSightComp))
	{
		return false;
	}

	return GuardSightComp->IsWithinBinocularVisionAngle(Target);
}
