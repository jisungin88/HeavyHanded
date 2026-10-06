// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTTask_AttemptArrest.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"
#include "Character/GuardCharacter.h"
#include "Character/BaseCharacter.h"


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

	// [체포 추가] 체포 판정은 서버 전용이며, 이미 다른 대상을 감시하는 경비는 새 체포를 시작하지 않는다.
	if (!IsValid(GuardAIController) || !GuardAIController->HasAuthority() || GuardAIController->IsInCustody() || !IsValid(BlackboardComp))
	{
		return EBTNodeResult::Failed;
	}

	// 경비 Pawn과 Blackboard에 저장된 체포 대상 Actor를 가져온다.
	APawn* GuardPawn = GuardAIController->GetPawn();
	// [체포 추가] 이동 무력화 상태를 가진 플레이어 기본 클래스만 체포할 수 있도록 대상을 한정한다.
	ABaseCharacter* TargetActor = Cast<ABaseCharacter>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));

	// [체포 추가] 다른 경비가 먼저 완료한 플레이어에게 다시 체포 애니메이션·타이머를 시작하지 않는다.
	if (!IsValid(GuardPawn) || !IsValid(TargetActor) || TargetActor->IsRestrained())
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
	ArrestTarget = TargetActor;
	// [체포 추가] 완료 판정은 이 시점에 저장한 대상에만 적용한다. 체포 중 타겟 교체는 완료 시 실패 처리한다.

	//0928 추가
	// 서버의 체포 애니메이션 상태를 변경하고 클라이언트에도 복제한다.
	if (AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(GuardPawn); IsValid(GuardCharacter))
	{
		GuardCharacter->SetArresting(true);
	}

	// 설정된 체포 시간만큼 기다린 뒤 최종 체포 판정을 수행한다.
	// [체포 추가] OwnerComp의 수명을 타이머가 보장하지 않으므로 약한 참조를 사용한다.
	// 설정이 0초여도 타이머가 삭제되어 태스크가 영구 대기하지 않도록 최소 양수 간격을 적용한다.
	GetWorld()->GetTimerManager().SetTimer(ArrestTimerHandle, FTimerDelegate::CreateUObject(this, &UBTTask_AttemptArrest::FinishArrest, TWeakObjectPtr<UBehaviorTreeComponent>(&OwnerComp)), FMath::Max(ArrestDuration, KINDA_SMALL_NUMBER), false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, ArrestDuration, FColor::Yellow, FString::Printf(TEXT("[%s] 체포 시도 중... %.1초"), *GetNameSafe(GuardPawn), ArrestDuration));
	}

	// 타이머가 끝날 때까지 Task를 계속 실행 상태로 유지한다.
	return EBTNodeResult::InProgress;
}

void UBTTask_AttemptArrest::FinishArrest(TWeakObjectPtr<UBehaviorTreeComponent> OwnerCompPtr)
{
	UBehaviorTreeComponent* OwnerComp = OwnerCompPtr.Get();
	// Behavior Tree가 이미 종료되었거나 유효하지 않다면 판정을 진행하지 않는다.
	if (!IsValid(OwnerComp))
	{
		return;
	}

	// 현재 AI 컨트롤러와 Blackboard를 가져온다.
	AGuardAIController* AIController = Cast<AGuardAIController>(OwnerComp->GetAIOwner());
	UBlackboardComponent* BlackboardComp = OwnerComp->GetBlackboardComponent();

	if (!IsValid(AIController) || !AIController->HasAuthority() || !IsValid(BlackboardComp))
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 최종 체포 판정에 사용할 경비 Pawn과 체포 대상 Actor를 가져온다.
	APawn* GuardPawn = AIController->GetPawn();
	ABaseCharacter* TargetActor = ArrestTarget;
	// [체포 추가] 보관한 시작 대상을 로컬로 옮긴 뒤 태스크의 참조를 비운다.
	// 성공·실패 어느 쪽으로 끝나도 다음 실행에 이전 대상이 남지 않게 한다.
	ArrestTarget = nullptr;


	//0928 추가
	// 체포 판정 시간이 끝났으므로 체포 애니메이션을 종료한다.
	if (AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(GuardPawn); IsValid(GuardCharacter))
	{
		GuardCharacter->SetArresting(false);
	}


	// [체포 추가] 타이머 동안 대상이 사라졌거나 다른 경비가 체포했거나 타겟이 교체되면 실패한다.
	// 유효할 때만 아래 기존 거리 검사를 수행한다. 시작·완료 거리 검사이며 매 틱 거리 검사는 아니다.
	if (!IsValid(GuardPawn) || !IsValid(TargetActor) || TargetActor->IsRestrained() || BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor) != TargetActor)
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

	// 서버에서 선점한다. 같은 틱에 여러 경비가 완료해도 한 경비만 감시를 맡는다.
	// [체포 추가] 기존 완료 로그만 출력하던 지점에 실제 체포 상태 등록·이동 차단을 연결한다.
	// 라운드 종료용 MarkArrested/State.Arrested는 호출하지 않는다.
	if (!TargetActor->TryRestrain(Cast<AGuardCharacter>(GuardPawn)))
	{
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
	// [체포 추가] 현재 태스크를 먼저 성공으로 종료한 뒤 BT를 정지한다.
	// 체포 완료가 BT 중단에 의해 Abort로 처리되거나 다음 루프에서 재체포되는 것을 막는다.
	AIController->BeginCustody(TargetActor);
}

EBTNodeResult::Type UBTTask_AttemptArrest::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// [체포 추가] 시도 취소는 완료 체포의 해제가 아니다. 아직 완료되지 않은 태스크 대상만 비운다.
	ArrestTarget = nullptr;
	// 체포 시도 중 Behavior Tree가 중단되면 대기 중인 체포 타이머를 제거한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArrestTimerHandle);
	}

	// 0928 추가
	// 체포 Task가 중단되었으므로 체포 애니메이션도 즉시 종료한다.
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		if (AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(AIController->GetPawn()); IsValid(GuardCharacter))
		{
			GuardCharacter->SetArresting(false);
		}
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
