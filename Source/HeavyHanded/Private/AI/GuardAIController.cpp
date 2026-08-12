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
	static const FName LastSeenTime(TEXT("LastSeenTime"));
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

	// 팀 시스템(IGenericTeamAgentInterface)을 별도로 구현하지 않았기 때문에,
	// 기본 설정(bDetectEnemies만 true)으로는 플레이어가 "중립"으로 판정되어 전혀 감지되지 않는다.
	// 소속과 무관하게 전부 감지하도록 명시적으로 켠다.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

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

	// SearchStartTime/LastSeenTime 기본값이 0.0이면, 게임 시작 직후 몇 초 동안
	// "한 번도 감지 안 했는데 타임아웃 조건이 우연히 참"이 되는 문제가 생길 수 있다.
	// 아주 먼 과거 값으로 초기화해 실제로 감지되기 전까지는 항상 타임아웃이 만료된 상태로 둔다.
	if (IsValid(BlackboardComp))
	{
		constexpr float FarPast = -100000.f;
		BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, FarPast);
		BlackboardComp->SetValueAsFloat(GuardAIKeys::LastSeenTime, FarPast);
	}

	// SelfActor / GuardType / AIState는 더 이상 Blackboard에 두지 않는다.
	// - Self는 OwnerComp.GetAIOwner()->GetPawn()으로 즉시 조회 가능
	// - GuardType은 이 컨트롤러의 UPROPERTY(GuardType)를 Cast<AGuardAIController>로 직접 참조
	// - 상태(Patrol/Investigate/Pursue)는 BT의 어느 브랜치가 실행 중인지 자체로 표현 (Decorator가 CanSeeTarget/DetectionGauge를 직접 판정)

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AGuardAIController::OnTargetPerceptionUpdated);

	// 시작 시 첫 순찰 지점을 미리 채워둔다 (비어있는 채로 Move To가 실행되는 것을 방지).
	SelectNextPatrolPoint();

	// BP_GuardAIController 는 data only 블루프린트라 그래프에서 대신 호출할 곳이 없고,
	// bStartAILogicOnPossess 도 BrainComponent 가 있어야 의미가 있다(그 컴포넌트를
	// 만들어주는 게 바로 이 호출이다). 여기서 부르지 않으면 BT 가 아예 시작되지 않는다.
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

			// 시야 경계에서 감지가 프레임 단위로 깜빡여도 추격을 바로 이탈하지 않도록,
			// 실제로 "본" 순간마다 시각을 갱신한다. BT 추격 브랜치의
			// Check Search Timeout(TimeKeyName=LastSeenTime, TimeoutSeconds=1.5)이 이 값을 읽는다.
			// 이 write 가 없으면 OnPossess 의 초기값(-100000)이 그대로 남아
			// 추격 조건이 영구히 거짓이 된다.
			BlackboardComp->SetValueAsFloat(GuardAIKeys::LastSeenTime, GetWorld()->GetTimeSeconds());
		}
		else
		{
			// 시야를 잃은 순간 = 수색 타이머 시작점
			BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, GetWorld()->GetTimeSeconds());
		}
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		// 소실(감지 종료) 이벤트에서는 위치가 유효하지 않을 수 있다.
		// 실제로 소리를 "들은" 순간에만 SoundTargetActor/InvestigateLocation/SearchStartTime을 갱신한다.
		if (Stimulus.WasSuccessfullySensed())
		{
			BlackboardComp->SetValueAsObject(GuardAIKeys::SoundTargetActor, Actor);
			BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, Stimulus.StimulusLocation);
			BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, GetWorld()->GetTimeSeconds());
		}
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

	const int32 PointCount = IsValid(GuardPawn) ? GuardPawn->GetPatrolPointCount() : 0;
	if (!IsValid(GuardPawn) || !IsValid(BlackboardComp) || PointCount == 0)
	{
		return;
	}

	// 첫 호출(-1)은 항상 0번 지점에서 시작.
	if (CurrentPatrolIndex < 0)
	{
		CurrentPatrolIndex = 0;
	}
	else if (PointCount == 1)
	{
		CurrentPatrolIndex = 0;
	}
	else
	{
		switch (GuardPawn->PatrolPattern)
		{
		case EPatrolPattern::Loop:
			CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PointCount;
			break;

		case EPatrolPattern::PingPong:
			if (bPatrolMovingForward)
			{
				CurrentPatrolIndex++;
				if (CurrentPatrolIndex >= PointCount - 1)
				{
					CurrentPatrolIndex = PointCount - 1;
					bPatrolMovingForward = false; // 끝에 도달 -> 역방향으로 전환
				}
			}
			else
			{
				CurrentPatrolIndex--;
				if (CurrentPatrolIndex <= 0)
				{
					CurrentPatrolIndex = 0;
					bPatrolMovingForward = true; // 처음으로 복귀 -> 정방향으로 전환
				}
			}
			break;

		case EPatrolPattern::Random:
			{
				// 직전 지점을 제외하고 뽑아서, 같은 자리에 멈춰있는 것처럼 보이는 걸 방지.
				int32 NextIndex = CurrentPatrolIndex;
				while (NextIndex == CurrentPatrolIndex)
				{
					NextIndex = FMath::RandRange(0, PointCount - 1);
				}
				CurrentPatrolIndex = NextIndex;
			}
			break;
		}
	}

	FVector NextLocation;
	if (GuardPawn->GetPatrolLocation(CurrentPatrolIndex, NextLocation))
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::PatrolLocation, NextLocation);
	}
}
