// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_AttemptArrest.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"

UBTTask_AttemptArrest::UBTTask_AttemptArrest()
{
	NodeName = TEXT("Attempt Arrest");

	// 체포 시도 중 사용하는 타이머 상태를 Task 인스턴스별로 독립적으로 관리한다.
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_AttemptArrest::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 현재 Behavior Tree를 실행 중인 경비 AI 컨트롤러를 가져온다.
	AGuardAIController* GuardAIController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());

	// 체포 대상이 저장된 Blackboard를 가져온다.
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!IsValid(GuardAIController) || !IsValid(BlackboardComp))
	{
		return EBTNodeResult::Failed;
	}

	// 경비 Pawn과 Blackboard에 저장된 체포 대상 Actor를 가져온다.
	APawn* GuardPawn = GuardAIController->GetPawn();
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));

	if (!IsValid(GuardPawn) || !IsValid(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	// 체포 시작 시점의 경비와 대상 사이 거리를 확인한다.
	const float Distance = FVector::Dist(GuardPawn->GetActorLocation(), TargetActor->GetActorLocation());

	UE_LOG(LogGuardAI, Log, TEXT("[%s] 체포 시도 시작: Target=%s Distance=%.1f Range=%.1f Duration=%.1f"),
		*GetNameSafe(GuardPawn), *GetNameSafe(TargetActor), Distance, ArrestRange, ArrestDuration);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("[%s] 체포 시도 시작 Distance=%.1f"), *GetNameSafe(GuardPawn), Distance));
	}

	// 체포 가능 거리 밖이라면 체포 시도를 실패 처리한다.
	if (Distance > ArrestRange)
	{
		UE_LOG(LogGuardAI, Log, TEXT("[%s] 체포 시도 실패: 거리 초과 Distance=%.1f Range=%.1f"),
			*GetNameSafe(GuardPawn), Distance, ArrestRange);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("[%s] 체포 실패: 거리 초과"), *GetNameSafe(GuardPawn)));
		}

		return EBTNodeResult::Failed;
	}

	// 체포 판정이 끝날 때까지 경비의 이동을 중지한다.
	GuardAIController->StopMovement();

	// 설정된 체포 시간만큼 기다린 뒤 최종 체포 판정을 수행한다.
	GetWorld()->GetTimerManager().SetTimer(ArrestTimerHandle, FTimerDelegate::CreateUObject(this, &UBTTask_AttemptArrest::FinishArrest, &OwnerComp), ArrestDuration, false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, ArrestDuration, FColor::Yellow, FString::Printf(TEXT("[%s] 체포 시도 중... %.1초"), *GetNameSafe(GuardPawn), ArrestDuration));
	}

	// 타이머가 끝날 때까지 Task를 계속 실행 상태로 유지한다.
	return EBTNodeResult::InProgress;
}

void UBTTask_AttemptArrest::FinishArrest(UBehaviorTreeComponent* OwnerComp)
{
	// Behavior Tree가 이미 종료되었거나 유효하지 않다면 판정을 진행하지 않는다.
	if (!IsValid(OwnerComp))
	{
		return;
	}

	// 현재 AI 컨트롤러와 Blackboard를 가져온다.
	AAIController* AIController = OwnerComp->GetAIOwner();
	UBlackboardComponent* BlackboardComp = OwnerComp->GetBlackboardComponent();

	if (!IsValid(AIController) || !IsValid(BlackboardComp))
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 최종 체포 판정에 사용할 경비 Pawn과 체포 대상 Actor를 가져온다.
	APawn* GuardPawn = AIController->GetPawn();
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));

	if (!IsValid(GuardPawn) || !IsValid(TargetActor))
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 체포 시간이 끝난 시점의 경비와 대상 사이 거리를 다시 확인한다.
	const float Distance = FVector::Dist(GuardPawn->GetActorLocation(), TargetActor->GetActorLocation());

	// 체포 중 대상이 체포 가능 거리 밖으로 이동했다면 체포에 실패한다.
	if (Distance > ArrestRange)
	{
		UE_LOG(LogGuardAI, Warning, TEXT("[%s] 체포 실패: 체포 중 거리 이탈 Target=%s Distance=%.1f Range=%.1f"),
			*GetNameSafe(GuardPawn), *GetNameSafe(TargetActor), Distance, ArrestRange);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("[%s] 체포 실패: 거리 이탈 Distance=%.1f"), *GetNameSafe(GuardPawn), Distance));
		}

		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 체포 시간 동안 대상이 범위 안에 있었다면 체포를 완료한다.
	UE_LOG(LogGuardAI, Warning, TEXT("[%s] 체포 완료: Target=%s Distance=%.1f"),
		*GetNameSafe(GuardPawn), *GetNameSafe(TargetActor), Distance);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("[%s] 체포 완료! Target=%s"), *GetNameSafe(GuardPawn), *GetNameSafe(TargetActor)));
	}

	// 체포 Task를 성공으로 종료하고 Behavior Tree의 다음 노드로 진행한다.
	FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_AttemptArrest::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 체포 시도 중 Behavior Tree가 중단되면 대기 중인 체포 타이머를 제거한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArrestTimerHandle);
	}

	// 체포 Task가 중단된 상황을 로그로 확인한다.
	UE_LOG(LogGuardAI, Log, TEXT("[%s] 체포 시도 중단"),
		*GetNameSafe(OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("[%s] 체포 시도 중단"), *GetNameSafe(OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr)));
	}

	// 부모 Task의 Abort 처리를 수행하고 그 결과를 반환한다.
	return Super::AbortTask(OwnerComp, NodeMemory);
}
