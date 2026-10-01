#include "AI/BTTask_FinishSearch.h"
#include "AI/GuardAIController.h"
#include "AI/GuardPatrolAComponent.h"
#include "AI/GuardBlackboardKeys.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_FinishSearch::UBTTask_FinishSearch()
{
	NodeName = TEXT("Finish Search");
	bCreateNodeInstance = true;
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_FinishSearch::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* Controller = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(BlackboardComp))
	{
		return EBTNodeResult::Failed;
	}

	const float SearchStartTime = BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime);
	const float Now = Controller->GetWorld()->GetTimeSeconds();
	if (BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget) || SearchStartTime < 0.0f || FMath::IsNearlyEqual(SearchStartTime, CompletedSearchStartTime) || Now - SearchStartTime < SearchTimeoutSeconds)
	{
		Controller->LogSearchTransitionDebug(TEXT("FinishRejected"), FString::Printf(TEXT("Failed: See=%d NoSession=%d AlreadyCompleted=%d TimeRemaining=%d Timeout=%.2f CompletedStart=%.3f"), BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget), SearchStartTime < 0.0f, FMath::IsNearlyEqual(SearchStartTime, CompletedSearchStartTime), Now - SearchStartTime < SearchTimeoutSeconds, SearchTimeoutSeconds, CompletedSearchStartTime));
		return EBTNodeResult::Failed;
	}

	ActiveSearchStartTime = SearchStartTime;
	Controller->LogSearchTransitionDebug(TEXT("FinishStarted"), TEXT("마무리 수색 실제 실행"));
	Controller->SetAIState(EGuardAIState::Search);
	bWaiting = true;
	WaitStartTime = Now;

	// 시야 기반 마지막 목격 위치가 없으면 현재 자리에서 마무리 대기한다.
	if (BlackboardComp->IsVectorValueSet(GuardAIKeys::LastKnownLocation))
	{
		const FVector LastKnownLocation = BlackboardComp->GetValueAsVector(GuardAIKeys::LastKnownLocation);
		if (IsValid(Controller->GuardPatrolComp))
		{
			constexpr int32 MaxMoveAttempts = 3;
			for (int32 Attempt = 0; Attempt < MaxMoveAttempts; ++Attempt)
			{
				FVector ReachableLocation;
				if (!Controller->GuardPatrolComp->FindReachableSearchLocation(LastKnownLocation, ReachableLocation, Attempt == 0))
				{
					break;
				}
				// 마지막 목격 위치는 유지하고 표시와 이동 목적지만 보정한다.
				BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, ReachableLocation);
				const EPathFollowingRequestResult::Type Result = Controller->MoveToLocation(ReachableLocation, AcceptanceRadius, true, true, true, true, nullptr, false);
				bWaiting = Result != EPathFollowingRequestResult::RequestSuccessful;
				Controller->LogSearchTransitionDebug(TEXT("FinishMoveRequest"), FString::Printf(TEXT("MoveRequest=%d Waiting=%d Goal=%s Attempt=%d"), static_cast<int32>(Result), bWaiting, *ReachableLocation.ToCompactString(), Attempt + 1));
				if (Result != EPathFollowingRequestResult::Failed)
				{
					break;
				}
				Controller->LogSearchTransitionDebug(TEXT("FinishMoveRetry"), FString::Printf(TEXT("마무리 이동 요청 실패: 다른 주변 후보 재시도 Attempt=%d"), Attempt + 1));
			}
		}
	}
	if (bWaiting && Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		// 모든 후보 이동 실패 시 남아 있는 이전 이동도 정지하고 현재 자리에서 대기한다.
		Controller->StopMovement();
	}

	UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 시간 만료: 마지막 목격 위치 확인 후 %.1f초 대기"), *GetNameSafe(Controller->GetPawn()), WaitSeconds);
	return EBTNodeResult::InProgress;
}

void UBTTask_FinishSearch::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	AGuardAIController* Controller = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(BlackboardComp))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 재발견 또는 새 조사 요청은 기존 마무리를 중단하고 BT가 다시 판단하게 한다.
	if (BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget) || !FMath::IsNearlyEqual(ActiveSearchStartTime, BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime)))
	{
		Controller->LogSearchTransitionDebug(TEXT("FinishInterrupted"), FString::Printf(TEXT("재발견 또는 새 세션: ActiveStart=%.3f"), ActiveSearchStartTime));
		Controller->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const float Now = Controller->GetWorld()->GetTimeSeconds();
	if (!bWaiting && Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		bWaiting = true;
		WaitStartTime = Now;
	}
	if (bWaiting && Now - WaitStartTime >= WaitSeconds)
	{
		CompletedSearchStartTime = ActiveSearchStartTime;
		Controller->LogSearchTransitionDebug(TEXT("FinishCompleted"), TEXT("마무리 대기 완료: Succeeded"));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_FinishSearch::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	if (IsValid(Controller) && Controller->HasAuthority())
	{
		if (AGuardAIController* GuardController = Cast<AGuardAIController>(Controller))
		{
			GuardController->LogSearchTransitionDebug(TEXT("FinishAbort"), TEXT("BT가 마무리 수색을 Abort함"));
		}
		Controller->StopMovement();
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}
