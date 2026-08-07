#include "AI/GuardAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Character/GuardCharacter.h"
// TODO: 실제 GameState 클래스명 및 소속 폴더로 교체
// (예: Core 폴더에 있다면 "Core/HeistGameState.h")
// #include "Core/HeistGameState.h"

namespace GuardAIKeys
{
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName LastKnownLocation(TEXT("LastKnownLocation"));
	static const FName PatrolLocation(TEXT("PatrolLocation"));
	static const FName CanSeeTarget(TEXT("CanSeeTarget"));
	static const FName IsInAttackRange(TEXT("IsInAttackRange"));
	static const FName SoundTargetActor(TEXT("SoundTargetActor"));
	static const FName WanderLocation(TEXT("WanderLocation"));
	static const FName DetectionGauge(TEXT("DetectionGauge"));
	static const FName InvestigateLocation(TEXT("InvestigateLocation"));
	static const FName SearchStartTime(TEXT("SearchStartTime"));
	static const FName HasLineOfFireOnTarget(TEXT("HasLineOfFireOnTarget"));
}

AGuardAIController::AGuardAIController()
{
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(*PerceptionComp);

	// Sight/Hearing 감지 설정은 생성자에서 기본값만 잡고,
	// 시야각·거리 등 세부 파라미터는 BP_GuardVariant_* 에서 GuardType별로 override.
	UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	UAISenseConfig_Hearing* HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	PerceptionComp->ConfigureSense(*SightConfig);
	PerceptionComp->ConfigureSense(*HearingConfig);
}

void AGuardAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!IsValid(BehaviorTreeAsset))
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = nullptr;
	UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BlackboardComp);

	// SelfActor / GuardType / AIState는 더 이상 Blackboard에 두지 않는다.
	// - Self는 OwnerComp.GetAIOwner()->GetPawn()으로 즉시 조회 가능
	// - GuardType은 이 컨트롤러의 UPROPERTY(GuardType)를 Cast<AGuardAIController>로 직접 참조
	// - 상태(Patrol/Investigate/Pursue)는 BT의 어느 브랜치가 실행 중인지 자체로 표현 (Decorator가 CanSeeTarget/DetectionGauge를 직접 판정)

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AGuardAIController::OnTargetPerceptionUpdated);

	// 시작 시 첫 순찰 지점을 미리 채워둔다 (비어있는 채로 Move To가 실행되는 것을 방지).
	SelectNextPatrolPoint();

	RunBehaviorTree(BehaviorTreeAsset);
}

void AGuardAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 클라이언트를 신뢰하지 않는다: 지각 판정 자체가 서버 시뮬레이션 결과이므로
	// 이 콜백은 서버에서만 의미 있는 Blackboard 갱신을 수행해야 한다.
	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(Actor) || !IsValid(GetBlackboardComponent()))
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		BlackboardComp->SetValueAsBool(GuardAIKeys::CanSeeTarget, Stimulus.WasSuccessfullySensed());
		if (Stimulus.WasSuccessfullySensed())
		{
			BlackboardComp->SetValueAsObject(GuardAIKeys::TargetActor, Actor);
			BlackboardComp->SetValueAsVector(GuardAIKeys::LastKnownLocation, Stimulus.StimulusLocation);
		}
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		BlackboardComp->SetValueAsObject(GuardAIKeys::SoundTargetActor, Actor);
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, Stimulus.StimulusLocation);
	}
}

float AGuardAIController::GetWorldAlertLevel() const
{
	// TODO: 임시 구현. 정식 버전에서는 UAlertComponent::GetAlertGauge()로 교체.
	// if (const AHeistGameState* GS = GetWorld()->GetGameState<AHeistGameState>())
	// {
	//     return GS->WorldAlertLevel;
	// }
	return 0.f;
}

void AGuardAIController::SelectNextPatrolPoint()
{
	const AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(GetPawn());
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();

	if (!IsValid(GuardPawn) || !IsValid(BlackboardComp) || GuardPawn->GetPatrolPointCount() == 0)
	{
		return;
	}

	CurrentPatrolIndex = (CurrentPatrolIndex + 1) % GuardPawn->GetPatrolPointCount();

	FVector NextLocation;
	if (GuardPawn->GetPatrolLocation(CurrentPatrolIndex, NextLocation))
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::PatrolLocation, NextLocation);
	}
}
