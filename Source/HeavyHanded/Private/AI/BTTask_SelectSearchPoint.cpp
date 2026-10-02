#include "AI/BTTask_SelectSearchPoint.h"
#include "AI/GuardAIController.h"
#include "AI/GuardTypes.h"
#include "AI/GuardPatrolAComponent.h"
#include "AI/GuardBlackboardKeys.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AITypes.h"
#include "Engine/World.h"

UBTTask_SelectSearchPoint::UBTTask_SelectSearchPoint()
{
	NodeName = TEXT("Select Search Point");
	bCreateNodeInstance = true;
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_SelectSearchPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(GuardController))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("AGuardAIController 가 아니라 수색 지점을 고를 수 없다 (현재 컨트롤러: %s)."),
			*GetNameSafe(OwnerComp.GetAIOwner()));
		return EBTNodeResult::Failed;
	}

	// 횟수 소진은 종료하고, 후보 탐색 실패는 남은 시간 안에서 같은 단계를 재시도한다.
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!GuardController->HasAuthority() || !IsValid(BlackboardComp) || !IsValid(GuardController->GuardPatrolComp))
	{
		return EBTNodeResult::Failed;
	}
	ActiveSessionStart = BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime);
	if (GuardController->SelectNextAction(EGuardAIState::Search))
	{
		return EBTNodeResult::Succeeded;
	}
	if (GuardController->GuardPatrolComp->IsSearchSweepExhausted() || !GuardController->GuardPatrolComp->IsSearchTimeRemaining(ActiveSessionStart, 12.f))
	{
		return EBTNodeResult::Failed;
	}
	// 후보가 없어도 바로 순찰로 넘어가지 않고 남은 시간 안에서 다시 탐색한다.
	GuardController->GuardPatrolComp->BeginSearchAttempt();
	GuardController->GuardPatrolComp->ReportSearchAttempt(false, 0.25f, FAISystem::InvalidLocation);
	NextRetryTime = GuardController->GetWorld()->GetTimeSeconds() + 0.25f;
	return EBTNodeResult::InProgress;
}

void UBTTask_SelectSearchPoint::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	AGuardAIController* Controller = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(BlackboardComp) || !IsValid(Controller->GuardPatrolComp))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (!FMath::IsNearlyEqual(ActiveSessionStart, BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime)) || !Controller->GuardPatrolComp->IsSearchTimeRemaining(ActiveSessionStart, 12.f))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (Controller->GetWorld()->GetTimeSeconds() >= NextRetryTime)
	{
		const EBTNodeResult::Type Result = ExecuteTask(OwnerComp, NodeMemory);
		if (Result != EBTNodeResult::InProgress)
		{
			FinishLatentTask(OwnerComp, Result);
		}
	}
}
