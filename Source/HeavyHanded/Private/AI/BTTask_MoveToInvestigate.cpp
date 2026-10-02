#include "AI/BTTask_MoveToInvestigate.h"
#include "AIController.h"
#include "AI/GuardAIController.h"
#include "AI/GuardPatrolAComponent.h"
#include "AI/GuardBlackboardKeys.h"
#include "AITypes.h"
#include "Engine/World.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"




UBTTask_MoveToInvestigate::UBTTask_MoveToInvestigate()
{
	NodeName = TEXT("Move To Investigate");
	BlackboardKey.SelectedKeyName = TEXT("InvestigateLocation");

	// 도착까지 기다려야 하므로 틱을 받는다.
	bNotifyTick = true;
	bCreateNodeInstance = true;
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
	ActiveSessionStart = BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime);
	MoveAttemptStartTime = AIController->GetWorld()->GetTimeSeconds();
	bWaitingForRetry = false;
	if (!FAISystem::IsValidLocation(InvestigateLocation))
	{
		AIController->LogSearchTransitionDebug(TEXT("InvestigateInvalidGoal"), TEXT("수색 이동 Failed: 목표 위치 무효"));
		// 아직 조사 지점이 기록된 적이 없다. 실패로 돌려 순찰 브랜치로 넘긴다.
		return EBTNodeResult::Failed;
	}

	EPathFollowingRequestResult::Type RequestResult = EPathFollowingRequestResult::Failed;
	AIController->GuardPatrolComp->BeginSearchAttempt();
	constexpr int32 MaxMoveAttempts = 3;
	for (int32 Attempt = 0; Attempt < MaxMoveAttempts; ++Attempt)
	{
		FVector ReachableLocation;
		if (!AIController->GuardPatrolComp->FindReachableSearchLocation(InvestigateLocation, ReachableLocation, Attempt == 0))
		{
			AIController->GuardPatrolComp->ReportSearchAttempt(false, AIController->GetWorld()->GetTimeSeconds() - MoveAttemptStartTime, FAISystem::InvalidLocation);
			break;
		}

		// 블랙보드에는 실제 이동 목적지만 기록하고 목격 위치와 세션 시각은 유지한다.
		BlackboardComp->SetValueAsVector(BlackboardKey.SelectedKeyName, ReachableLocation);
		ActiveGoal = ReachableLocation;
		RequestResult = AIController->MoveToLocation(ReachableLocation, AcceptanceRadius, true, true, true, true, nullptr, false);
		AIController->LogSearchTransitionDebug(TEXT("InvestigateMoveRequest"), FString::Printf(TEXT("MoveRequest=%d Goal=%s Attempt=%d"), static_cast<int32>(RequestResult), *ReachableLocation.ToCompactString(), Attempt + 1));
		if (RequestResult != EPathFollowingRequestResult::Failed)
		{
			break;
		}
		AIController->LogSearchTransitionDebug(TEXT("InvestigateMoveRetry"), FString::Printf(TEXT("이동 요청 실패: 다른 주변 후보 재시도 Attempt=%d"), Attempt + 1));
		AIController->GuardPatrolComp->ReportSearchAttempt(false, AIController->GetWorld()->GetTimeSeconds() - MoveAttemptStartTime, ReachableLocation);
	}

	switch (RequestResult)
	{
	case EPathFollowingRequestResult::RequestSuccessful:
		return EBTNodeResult::InProgress;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		AIController->GuardPatrolComp->ReportSearchAttempt(true, 0.f, ActiveGoal);
		return EBTNodeResult::Succeeded;

	default:
		// 경로를 못 냈다 - NavMesh 밖이거나 도달 불가능한 지점.
		UE_LOG(LogTemp, Warning, TEXT("[%s] 조사 지점 %s 로 경로를 내지 못했다 (NavMesh 밖?)."),
			*GetNameSafe(AIController->GetPawn()), *InvestigateLocation.ToCompactString());
		bWaitingForRetry = true;
		NextRetryTime = AIController->GetWorld()->GetTimeSeconds() + 0.25f;
		return EBTNodeResult::InProgress;
	}
}

void UBTTask_MoveToInvestigate::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	AGuardAIController* Controller = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(BlackboardComp) || !IsValid(Controller->GuardPatrolComp))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (!FMath::IsNearlyEqual(ActiveSessionStart, BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime)))
	{
		Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	const float Now = Controller->GetWorld()->GetTimeSeconds();
	bool bReached = false;
	if (!bWaitingForRetry)
	{
		Controller->GuardPatrolComp->ObserveSearchAttemptDuration(Now - MoveAttemptStartTime);
		if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			const UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();
			bReached = IsValid(PathFollowing) && PathFollowing->DidMoveReachGoal();
			Controller->GuardPatrolComp->ReportSearchAttempt(bReached, Now - MoveAttemptStartTime, ActiveGoal);
			if (!bReached)
			{
				bWaitingForRetry = true;
				NextRetryTime = Now + 0.25f;
			}
		}
	}
	// 최신 이동 결과를 먼저 반영해 정상 이동 중에는 빠른 실패 연장을 허용하지 않는다.
	if (!Controller->GuardPatrolComp->IsSearchTimeRemaining(ActiveSessionStart, 12.f))
	{
		Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (bReached)
	{
		Controller->LogSearchTransitionDebug(TEXT("InvestigateIdle"), TEXT("수색 목적지 도착: 두리번 대기로 진행"));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
	if (bWaitingForRetry && Now >= NextRetryTime)
	{
		const EBTNodeResult::Type Result = ExecuteTask(OwnerComp, NodeMemory);
		if (Result != EBTNodeResult::InProgress)
		{
			FinishLatentTask(OwnerComp, Result);
		}
	}
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
