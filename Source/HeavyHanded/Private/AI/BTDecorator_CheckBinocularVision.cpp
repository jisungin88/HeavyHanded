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
	// 각도는 CanSeeTarget이 바뀌지 않아도 변한다. 경비별로 현재 각도 조건을 계속 관찰한다.
	bCreateNodeInstance = true;
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
	FlowAbortMode = EBTFlowAbortMode::Both;
}

FString UBTDecorator_CheckBinocularVision::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s\n보이는 대상이 양안각 안에 있을 때만 바라보기\n각도 변화 지속 감시 / 관찰 키: %s"), *Super::GetStaticDescription(), *BlackboardKey.SelectedKeyName.ToString());
}

void UBTDecorator_CheckBinocularVision::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	bLastConditionResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
}

void UBTDecorator_CheckBinocularVision::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	const bool bConditionResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
	if (bConditionResult != bLastConditionResult)
	{
		bLastConditionResult = bConditionResult;
		OwnerComp.RequestExecution(this);
	}
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
	if (!IsValid(Target) || !BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget))
	{
		return false;
	}

	const UGuardSightAComponent* GuardSightComp = AIController->FindComponentByClass<UGuardSightAComponent>();
	if (!IsValid(GuardSightComp))
	{
		return false;
	}

	return GuardSightComp->IsWithinBinocularVisionAngle(Target);
}
