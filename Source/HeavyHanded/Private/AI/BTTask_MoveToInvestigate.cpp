#include "AI/BTTask_MoveToInvestigate.h"
#include "AIController.h"
#include "AITypes.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/GuardTypes.h"

UBTTask_MoveToInvestigate::UBTTask_MoveToInvestigate()
{
	NodeName = TEXT("Move To Investigate");
	BlackboardKey.SelectedKeyName = TEXT("InvestigateLocation");

	// 도착까지 기다려야 하므로 틱을 받는다.
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_MoveToInvestigate::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!IsValid(AIController) || !IsValid(BlackboardComp))
	{
		return EBTNodeResult::Failed;
	}

	const FVector InvestigateLocation = BlackboardComp->GetValueAsVector(BlackboardKey.SelectedKeyName);
	if (!FAISystem::IsValidLocation(InvestigateLocation))
	{
		// 아직 조사 지점이 기록된 적이 없다. 실패로 돌려 순찰 브랜치로 넘긴다.
		return EBTNodeResult::Failed;
	}

	const EPathFollowingRequestResult::Type RequestResult =
		AIController->MoveToLocation(InvestigateLocation, AcceptanceRadius);

	switch (RequestResult)
	{
	case EPathFollowingRequestResult::RequestSuccessful:
		return EBTNodeResult::InProgress;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		return EBTNodeResult::Succeeded;

	default:
		// 경로를 못 냈다 - NavMesh 밖이거나 도달 불가능한 지점.
		UE_LOG(LogGuardAI, Warning, TEXT("[%s] 조사 지점 %s 로 경로를 내지 못했다 (NavMesh 밖?)."),
			*GetNameSafe(AIController->GetPawn()), *InvestigateLocation.ToCompactString());
		return EBTNodeResult::Failed;
	}
}

void UBTTask_MoveToInvestigate::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	const AAIController* AIController = OwnerComp.GetAIOwner();
	if (!IsValid(AIController))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 도착이든 중단이든 이동이 끝나면 태스크를 마친다. 여기서 Succeeded 를 주더라도
	// 조사를 계속할지 순찰로 돌아갈지는 브랜치의 Check Search Timeout 이 판정한다.
	if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_MoveToInvestigate::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 상위 브랜치(추격)가 가로챈 경우. 진행 중이던 이동 명령을 남겨두면
	// 추격 브랜치의 Move To 와 경로가 충돌한다.
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		AIController->StopMovement();
	}

	return Super::AbortTask(OwnerComp, NodeMemory);
}
