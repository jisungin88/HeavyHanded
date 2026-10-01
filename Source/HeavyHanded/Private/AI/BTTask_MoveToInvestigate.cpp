#include "AI/BTTask_MoveToInvestigate.h"
#include "AIController.h"
#include "AI/GuardAIController.h"
#include "AI/GuardPatrolAComponent.h"
#include "AITypes.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"




UBTTask_MoveToInvestigate::UBTTask_MoveToInvestigate()
{
	NodeName = TEXT("Move To Investigate");
	BlackboardKey.SelectedKeyName = TEXT("InvestigateLocation");

	// 도착까지 기다려야 하므로 틱을 받는다.
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_MoveToInvestigate::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* AIController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!IsValid(AIController) || !AIController->HasAuthority() || !IsValid(BlackboardComp) || !IsValid(AIController->GuardPatrolComp))
	{
		return EBTNodeResult::Failed;
	}

	const FVector InvestigateLocation = BlackboardComp->GetValueAsVector(BlackboardKey.SelectedKeyName);
	if (!FAISystem::IsValidLocation(InvestigateLocation))
	{
		AIController->LogSearchTransitionDebug(TEXT("InvestigateInvalidGoal"), TEXT("수색 이동 Failed: 목표 위치 무효"));
		// 아직 조사 지점이 기록된 적이 없다. 실패로 돌려 순찰 브랜치로 넘긴다.
		return EBTNodeResult::Failed;
	}

	EPathFollowingRequestResult::Type RequestResult = EPathFollowingRequestResult::Failed;
	constexpr int32 MaxMoveAttempts = 3;
	for (int32 Attempt = 0; Attempt < MaxMoveAttempts; ++Attempt)
	{
		FVector ReachableLocation;
		if (!AIController->GuardPatrolComp->FindReachableSearchLocation(InvestigateLocation, ReachableLocation, Attempt == 0))
		{
			break;
		}

		// 블랙보드에는 실제 이동 목적지만 기록하고 목격 위치와 세션 시각은 유지한다.
		BlackboardComp->SetValueAsVector(BlackboardKey.SelectedKeyName, ReachableLocation);
		RequestResult = AIController->MoveToLocation(ReachableLocation, AcceptanceRadius, true, true, true, true, nullptr, false);
		AIController->LogSearchTransitionDebug(TEXT("InvestigateMoveRequest"), FString::Printf(TEXT("MoveRequest=%d Goal=%s Attempt=%d"), static_cast<int32>(RequestResult), *ReachableLocation.ToCompactString(), Attempt + 1));
		if (RequestResult != EPathFollowingRequestResult::Failed)
		{
			break;
		}
		AIController->LogSearchTransitionDebug(TEXT("InvestigateMoveRetry"), FString::Printf(TEXT("이동 요청 실패: 다른 주변 후보 재시도 Attempt=%d"), Attempt + 1));
	}

	switch (RequestResult)
	{
	case EPathFollowingRequestResult::RequestSuccessful:
		return EBTNodeResult::InProgress;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		return EBTNodeResult::Succeeded;

	default:
		// 경로를 못 냈다 - NavMesh 밖이거나 도달 불가능한 지점.
		UE_LOG(LogTemp, Warning, TEXT("[%s] 조사 지점 %s 로 경로를 내지 못했다 (NavMesh 밖?)."),
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

	//-----------------------------------------------------
	//0929 수정
	// 도착이든 이동 중단이든 이동이 끝나면 조사 브랜치로 제어를 넘긴다.
	// 조사 지속 여부와 순찰 복귀는 Check Search Timeout이 판정한다.
	if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		if (AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner()))
		{
			GuardController->LogSearchTransitionDebug(TEXT("InvestigateIdle"), TEXT("이동 Idle 감지: 수색 이동 태스크 Succeeded"));
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
	//-----------------------------------------------------
}

EBTNodeResult::Type UBTTask_MoveToInvestigate::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 상위 브랜치(추격)가 가로챈 경우. 진행 중이던 이동 명령을 남겨두면
	// 추격 브랜치의 Move To 와 경로가 충돌한다.
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		if (AGuardAIController* GuardController = Cast<AGuardAIController>(AIController))
		{
			GuardController->LogSearchTransitionDebug(TEXT("InvestigateAbort"), TEXT("BT가 일반 수색 이동을 Abort함"));
		}
		AIController->StopMovement();
	}

	return Super::AbortTask(OwnerComp, NodeMemory);
}
