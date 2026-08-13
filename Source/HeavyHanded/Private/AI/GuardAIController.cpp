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
#include "AI/GuardBlackboardKeys.h"
#include "Alert/AlertComponent.h"
#include "AITypes.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY(LogGuardAI);

AGuardAIController::AGuardAIController()
{
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(*PerceptionComp);

	// Sight/Hearing 감지 설정은 생성자에서 기본값만 잡고,
	// 시야각·거리 등 세부 파라미터는 BP_GuardAIController 및 그 파생 BP 에서
	// GuardType 별로 override 한다. 멤버(UPROPERTY)로 들고 있어야 디테일 패널에 뜬다.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));

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
		UE_LOG(LogGuardAI, Error,
			TEXT("[%s] BehaviorTreeAsset 이 비어 있다. BT 시작과 Perception 바인딩을 모두 건너뛴다. "
				 "BP_GuardAIController 의 Guard|AI > Behavior Tree Asset 을 확인할 것."),
			*GetNameSafe(InPawn));
		return;
	}

	UBlackboardComponent* BlackboardComp = nullptr;
	UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BlackboardComp);

	if (!IsValid(BlackboardComp))
	{
		UE_LOG(LogGuardAI, Error,
			TEXT("[%s] Blackboard 생성 실패. BT_Guards 에 Blackboard Asset 이 물려 있는지 확인할 것."),
			*GetNameSafe(InPawn));
	}

	// SearchStartTime/LastSeenTime 기본값이 0.0이면, 게임 시작 직후 몇 초 동안
	// "한 번도 감지 안 했는데 타임아웃 조건이 우연히 참"이 되는 문제가 생길 수 있다.
	// 아주 먼 과거 값으로 초기화해 실제로 감지되기 전까지는 항상 타임아웃이 만료된 상태로 둔다.
	if (IsValid(BlackboardComp))
	{
		constexpr float FarPast = -100000.f;
		BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, FarPast);
		BlackboardComp->SetValueAsFloat(GuardAIKeys::LastSeenTime, FarPast);
	}

	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AGuardAIController::OnTargetPerceptionUpdated);

	// 빙의한 폰의 PerceptionMeterComponent(소음 인지 게이지)를 찾아 OnPerceptionFull 을 구독한다.
	// 멤버 PerceptionMeter 에도 캐싱해 둬야 한다 - HandlePerceptionFull 에서 게이지를
	// 리셋(ResetPerception)할 때 이 멤버를 쓰는데, 로컬 변수에만 대입하고 멤버 대입을
	// 빠뜨리면 항상 nullptr 이라 리셋이 절대 호출되지 않는다. 그러면 래치가 안 풀려
	// 게이지가 100%에서 그대로 굳어 두 번째 소음부터는 OnPerceptionFull 이 다시 터지지 않는다.
	if (AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(InPawn))
	{
		PerceptionMeter = GuardPawn->FindComponentByClass<UPerceptionMeterComponent>();
		if (PerceptionMeter)
		{
			PerceptionMeter->OnPerceptionFull.AddDynamic(this, &AGuardAIController::HandlePerceptionFull);
		}
		else
		{
			UE_LOG(LogGuardAI, Error, TEXT("[%s] PerceptionMeterComponent 를 찾지 못했다. GuardCharacter 파생 폰인지 확인할 것."),
				*GetNameSafe(InPawn));
		}
	}

	// 시작 시 첫 순찰 지점을 미리 채워둔다
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
		// 시야 획득/상실이 초당 여러 번 뒤집히면 추격 브랜치가 그만큼 abort/restart 된다.
		// 눈으로 세기 어려우므로 상실이 실제로 몇 초 지속됐는지를 같이 찍는다.
		// 1초 미만이 반복되면 깜빡임, 수 초 단위면 정상적으로 놓친 것이다.
		const float NowSeconds = GetWorld()->GetTimeSeconds();

		if (Stimulus.WasSuccessfullySensed())
		{
			if (SightLostAtTime >= 0.f)
			{
				UE_LOG(LogGuardAI, Log, TEXT("[%s] 시야 획득: %s (직전 상실이 %.2f초 지속)"),
					*GetNameSafe(GetPawn()), *GetNameSafe(Actor), NowSeconds - SightLostAtTime);
			}
			else
			{
				UE_LOG(LogGuardAI, Log, TEXT("[%s] 시야 획득: %s (최초)"),
					*GetNameSafe(GetPawn()), *GetNameSafe(Actor));
			}

			SightLostAtTime = -1.f;
		}
		else
		{
			SightLostAtTime = NowSeconds;

			UE_LOG(LogGuardAI, Log, TEXT("[%s] 시야 상실: %s"),
				*GetNameSafe(GetPawn()), *GetNameSafe(Actor));
		}

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
		// 시야를 잃었다고 해서 여기서 SearchStartTime 을 쓰지 않는다.
		//
		// 쓰면 스쳐 지나가듯 한 번 보이기만 해도 조사가 켜진다. Guard.ini 는
		// Guard.State.Investigate 를 "인지 게이지가 가득 차" 진입하는 상태로 정의한다.
		// 그 조건은 BTService_UpdateDetectionGauge 가 게이지 100 인 동안 매 틱
		// SearchStartTime 을 밀어주는 것으로 이미 만족된다 - 시야를 잃는 순간
		// 그 값이 얼어붙어 자연스럽게 "수색 시작 시각"이 된다.
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

void AGuardAIController::HandlePerceptionFull(FVector LastNoiseLocation)
{
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

bool AGuardAIController::SelectNextSearchPoint()
{
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	const APawn* GuardPawn = GetPawn();

	if (!IsValid(BlackboardComp) || !IsValid(GuardPawn))
	{
		return false;
	}

	// 조사 지점은 LastKnownLocation(시야 전용 키)이 아니라 InvestigateLocation에서 읽는다.
	//
	// SearchStartTime을 갱신하는 곳이 세 군데다: 시야 기반 UBTService_UpdateDetectionGauge
	// (게이지 100% 도달), 소리 기반 HandlePerceptionFull(인지 게이지 100%),
	// 그리고 OnTargetPerceptionUpdated의 Hearing 분기(자극 1건). 셋 다 SearchStartTime을
	// 쓰는 바로 그 자리에서 InvestigateLocation도 같이 채워 넣으므로, 여기서 다시
	// LastKnownLocation을 읽으면 "시야로 진입한 조사"만 성립하고 소리로 들어온 조사는
	// LastKnownLocation이 비어 있어 매번 실패한다 — 실제로 경비를 한 번도 안 들켰는데
	// 소리만으로 게이지를 채우면 "마지막 목격 위치가 없어 수색을 시작할 수 없다" 로 막혔었다.
	const FVector SearchAnchor = BlackboardComp->GetValueAsVector(GuardAIKeys::InvestigateLocation);
	if (!FAISystem::IsValidLocation(SearchAnchor))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] 조사 지점이 없어 수색을 시작할 수 없다."), *GetNameSafe(GuardPawn));
		return false;
	}

	// SearchStartTime 이 바뀌었으면 새 조사다. 훑기 진행도를 초기화한다.
	const float SearchStartTime = BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime);
	if (!FMath::IsNearlyEqual(SearchStartTime, HandledSearchStartTime))
	{
		HandledSearchStartTime = SearchStartTime;
		CurrentSearchStep = -1;
	}

	++CurrentSearchStep;

	// 0번째는 조사 지점 자체(마지막 목격 지점 또는 소리 지점). 여기부터 확인하는 게 자연스럽다.
	if (CurrentSearchStep == 0)
	{
		// 이미 InvestigateLocation에 들어있는 값과 같지만, Blackboard 갱신 시점을
		// 명시적으로 남겨 다른 리스너(위젯 등)가 "조사 0단계 진입"을 관찰할 수 있게 한다.
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, SearchAnchor);

		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 시작 - 조사 지점 %s"),
			*GetNameSafe(GuardPawn), *SearchAnchor.ToCompactString());
		return true;
	}

	if (CurrentSearchStep > SearchSweepCount)
	{
		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 종료 - %d개 지점을 훑었다. 순찰로 복귀."),
			*GetNameSafe(GuardPawn), SearchSweepCount);
		return false;
	}

	// 조사 지점 주변에서 실제로 도달 가능한 지점만 고른다.
	// 무작위 오프셋을 그냥 더하면 벽 너머나 NavMesh 밖이 나와 Move To 가 실패한다.
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation SweepPoint;

	if (IsValid(NavSys) && NavSys->GetRandomReachablePointInRadius(SearchAnchor, SearchSweepRadius, SweepPoint))
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, SweepPoint.Location);

		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 %d/%d - %s"),
			*GetNameSafe(GuardPawn), CurrentSearchStep, SearchSweepCount,
			*SweepPoint.Location.ToCompactString());
		return true;
	}

	UE_LOG(LogGuardAI, Warning,
		TEXT("[%s] 조사 지점 %s 반경 %.0f 안에서 도달 가능한 수색 지점을 찾지 못했다."),
		*GetNameSafe(GuardPawn), *SearchAnchor.ToCompactString(), SearchSweepRadius);
	return false;
}

float AGuardAIController::GetWorldAlertLevel() const
{
	// UAlertComponent 는 GameState 에 런타임 부착되며 0~1 게이지를 들고 있다.
	// 여기서는 0~100 퍼센트로 바꿔 돌려준다 - Alert.ini 가 단계를 퍼센트로
	// 문서화하고 있어(Calm 0~33 / Suspicious 34~66 / Alerted 67~99 / Alarm 100)
	// BTDecorator_CheckWorldAlert 의 임계값을 기획서 숫자 그대로 쓸 수 있다.
	if (const UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		return Alert->GetAlertGauge01() * 100.f;
	}

	// GameState 에 아직 컴포넌트가 없다(리슨 서버 시작 직후 등). 경계도 0 으로 취급.
	return 0.f;
}

void AGuardAIController::SelectNextPatrolPoint()
{
	const AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(GetPawn());
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();

	if (!IsValid(GuardPawn))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] AGuardCharacter 가 아니라 순찰 지점을 읽을 수 없다 (현재 폰: %s)."),
			*GetName(), *GetNameSafe(GetPawn()));
		return;
	}

	if (!IsValid(BlackboardComp))
	{
		UE_LOG(LogGuardAI, Warning, TEXT("[%s] Blackboard 가 없어 PatrolLocation 을 쓸 수 없다."), *GetName());
		return;
	}

	const int32 PointCount = GuardPawn->GetPatrolPointCount();
	if (PointCount == 0)
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] PatrolPoints 가 비어 있다. EditInstanceOnly 라 레벨에 '배치된' 액터에만 값이 붙는다 "
				 "— 스폰된 경비라면 여기서 항상 비어 있다."),
			*GetNameSafe(GuardPawn));
		return;
	}

	// 아직 현재 목표에 도착하지 않았다면 지점을 넘기지 않는다.
	//
	// 이 함수는 순찰 브랜치에 진입할 때마다 호출되는데, 시야 획득으로 순찰이
	// abort 되고 상실 후 재개되는 것도 "새 진입"이다. 진입마다 전진시키면A
	// 경비가 플레이어를 한 번 볼 때마다 순찰 지점을 하나씩 건너뛴다.
	if (CurrentPatrolIndex >= 0)
	{
		FVector CurrentTarget;
		if (GuardPawn->GetPatrolLocation(CurrentPatrolIndex, CurrentTarget))
		{
			// Z 는 무시한다 - 지점 액터가 바닥에서 떠 있어도 도착 판정이 되도록.
			const float DistToCurrent = FVector::Dist2D(GuardPawn->GetActorLocation(), CurrentTarget);
			if (DistToCurrent > PatrolArrivalRadius)
			{
				// 가던 길을 계속 간다. Blackboard 값은 다시 써준다 —
				// 조사 브랜치를 거치는 동안 다른 값으로 덮였을 수 있다.
				BlackboardComp->SetValueAsVector(GuardAIKeys::PatrolLocation, CurrentTarget);
				return;
			}
		}
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

		// 정상 동작이면 순찰 지점에 도착할 때마다 한 번씩만 찍힌다.
		// 호출 간격(dt)이 프레임 단위이고 폰이 제자리면 브랜치가 abort/restart 를
		// 반복하는 것이고, dt 가 수 초 단위면 실제로 걸어서 도착하고 있는 것이다.
		const float Now = GetWorld()->GetTimeSeconds();
		const float DeltaSinceLast = (LastPatrolSelectTime < 0.f) ? -1.f : (Now - LastPatrolSelectTime);
		LastPatrolSelectTime = Now;

		const FVector PawnLocation = GuardPawn->GetActorLocation();

		UE_LOG(LogGuardAI, Log,
			TEXT("[%s] 순찰 지점 %d 선택: %s | dt=%.3fs | 폰 위치 %s | 남은 거리 %.0f"),
			*GetNameSafe(GuardPawn), CurrentPatrolIndex, *NextLocation.ToCompactString(),
			DeltaSinceLast, *PawnLocation.ToCompactString(),
			FVector::Dist(PawnLocation, NextLocation));
	}
	else
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] 순찰 지점 %d 의 위치를 얻지 못했다 (배열 항목이 비어 있는지 확인)."),
			*GetNameSafe(GuardPawn), CurrentPatrolIndex);
	}
}
