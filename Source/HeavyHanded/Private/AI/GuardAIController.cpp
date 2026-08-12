#include "AI/GuardAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Character/GuardCharacter.h"
#include "Noise/PerceptionMeterComponent.h"
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

	if (IsValid(BlackboardComp))
	{
		constexpr float FarPast = -100000.f;
		BlackboardComp->SetValueAsFloat(TEXT("SearchStartTime"), FarPast);
		BlackboardComp->SetValueAsFloat(TEXT("LastSeenTime"), FarPast);
	}

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AGuardAIController::OnTargetPerceptionUpdated);

	// ↓↓↓ 여기에 새로 추가 (지난번 작업에서 이미 넣으신 그 블록 바로 여기) ↓↓↓
	if (AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(InPawn))
	{
		if (UPerceptionMeterComponent* Meter = GuardPawn->FindComponentByClass<UPerceptionMeterComponent>())
		{
			Meter->OnPerceptionFull.AddDynamic(this, &AGuardAIController::HandlePerceptionFull);
			UE_LOG(LogTemp, Warning, TEXT("PerceptionMeter bound successfully!"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PerceptionMeter component NOT FOUND on GuardPawn!"));
		}
	}
	// ↑↑↑ 여기까지 ↑↑↑

	// 시작 시 첫 순찰 지점을 미리 채워둔다
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
			BlackboardComp->SetValueAsFloat(TEXT("SearchStartTime"), GetWorld()->GetTimeSeconds());
		}
	}
}

void AGuardAIController::HandlePerceptionFull(FVector LastNoiseLocation)
{
	//GuardAIController.cpp의 HandlePerceptionFull 함수 맨 위에 임시 로그 추가:
	UE_LOG(LogTemp, Warning, TEXT("HandlePerceptionFull CALLED! Location: %s"), *LastNoiseLocation.ToString());

	// 인지 게이지 판정은 서버 권위이므로 이 콜백도 서버에서만 의미가 있다 (OnTargetPerceptionUpdated와 동일한 이유)
	if (!HasAuthority())
	{
		return;
	}

	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, LastNoiseLocation);
		BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, GetWorld()->GetTimeSeconds());
	}

	// 리셋하지 않으면 래치가 풀리지 않아 경비가 영원히 100%에 박힌다 (PerceptionMeterComponent.h 참고)
	if (PerceptionMeter)
	{
		PerceptionMeter->ResetPerception();
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
