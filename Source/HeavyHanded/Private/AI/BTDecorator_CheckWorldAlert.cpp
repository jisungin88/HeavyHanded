#include "AI/BTDecorator_CheckWorldAlert.h"
#include "AIController.h"
#include "AI/GuardAIController.h"

UBTDecorator_CheckWorldAlert::UBTDecorator_CheckWorldAlert()
{
	NodeName = TEXT("Check World Alert");
	bCreateNodeInstance = true;
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
	FlowAbortMode = EBTFlowAbortMode::Both;
}

void UBTDecorator_CheckWorldAlert::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	bLastConditionResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
}

void UBTDecorator_CheckWorldAlert::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	const bool bConditionResult = CalculateRawConditionValue(OwnerComp, NodeMemory);
	if (bConditionResult != bLastConditionResult)
	{
		bLastConditionResult = bConditionResult;
		OwnerComp.RequestExecution(this);
	}
}

bool UBTDecorator_CheckWorldAlert::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(GuardController))
	{
		return false;
	}

	// 읽기 전용 조회. 값 변경은 서버 권한 로직(소음 판정 함수) 쪽에서만 일어난다.
	return AlertThreshold >= 100.f ? GuardController->IsWorldAlarmActive() : GuardController->GetWorldAlertLevel() >= AlertThreshold;
}
