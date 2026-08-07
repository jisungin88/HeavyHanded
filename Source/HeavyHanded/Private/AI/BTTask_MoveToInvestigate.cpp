#include "AI/BTTask_MoveToInvestigate.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_MoveToInvestigate::UBTTask_MoveToInvestigate()
{
	NodeName = TEXT("Move To Investigate");
	BlackboardKey.SelectedKeyName = TEXT("InvestigateLocation");
}

EBTNodeResult::Type UBTTask_MoveToInvestigate::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!IsValid(AIController) || !IsValid(BlackboardComp))
	{
		return EBTNodeResult::Failed;
	}

	const FVector InvestigateLocation = BlackboardComp->GetValueAsVector(TEXT("InvestigateLocation"));
	AIController->MoveToLocation(InvestigateLocation, AcceptanceRadius);

	// 수색 시작 시각 기록 -> Decorator_CheckSearchTimeout이 참조해 Return 전환 판정.
	BlackboardComp->SetValueAsFloat(TEXT("SearchStartTime"), AIController->GetWorld()->GetTimeSeconds());

	return EBTNodeResult::Succeeded;
}
