
// 헤더 정리 0907
// Guard AI
#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"
#include "AI/GuardSettings.h"

#include "AI/GuardSightAComponent.h"
#include "AI/GuardHearingAComponent.h"
#include "AI/GuardPatrolAComponent.h"


// Guard Character
#include "Character/GuardCharacter.h"

// Behavior Tree / Blackboard
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"

// AI Perception
#include "AITypes.h"
#include "Perception/AIPerceptionComponent.h"

// Navigation
#include "NavigationSystem.h"

// Gameplay / Game State
#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Alert/AlertComponent.h"

// Noise / Perception Meter
#include "Noise/PerceptionMeterComponent.h"

// Data
#include "Engine/DataTable.h"

// Character / Movement
#include "GameFramework/CharacterMovementComponent.h"

// World / Actor
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"

// UI
//#include "Components/WidgetComponent.h"
//#include "UI/DetectionGaugeWidget.h"
//#include "Kismet/GameplayStatics.h"





DEFINE_LOG_CATEGORY(LogGuardAI);

// 1. 생성자
AGuardAIController::AGuardAIController()
{
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SetPerceptionComponent(*PerceptionComp);


	// AC 추가 0908
	GuardPatrolComp = CreateDefaultSubobject<UGuardPatrolAComponent>(TEXT("GuardPatrol"));

	GuardSightComp = CreateDefaultSubobject<UGuardSightAComponent>(TEXT("GuardSight"));
	GuardHearingComp = CreateDefaultSubobject<UGuardHearingAComponent>(TEXT("GuardHearingComp"));


	// AAIController가 IGenericTeamAgentInterface를 이미 구현하고 있어(TeamID 멤버) 여기서는
	// 그 값만 채운다. 모든 경비를 같은 팀으로 묶어 서로 "우호"로 판정되게 한다.
	SetGenericTeamId(FGenericTeamId(1));

}

// 2. 초기화
void AGuardAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		// 두 경로가 다 필요하다 — 레벨에 처음부터 놓인 경비와 AGuardSpawner 가 나중에 뿌리는
		// 경비는 GameState 도착 시점이 다르다. 이미 와 있으면 지금 붙고, 아직이면 도착할 때 붙는다
		BindToGameState(World->GetGameState());

		GameStateSetHandle =
			World->GameStateSetEvent.AddUObject(this, &AGuardAIController::BindToGameState);
	}
}

// 3. 빙의 후 초기화
void AGuardAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);


	AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(InPawn);

	if (!IsValid(GuardPawn))
	{
		// 에러 로그
		return;
	}


	// PerceptionComp 넘겨주기 (추후 수정 필요)
	GuardSightComp->InitializeSightPerception(PerceptionComp);
	GuardHearingComp->InitializeHearingPerception(PerceptionComp);

	// 시야, 청각 활성화 여부 결정 (테스트용)
	GuardSightComp->SetSightEnabled(GuardPawn->bEnableSight);
	GuardHearingComp->SetHearingEnabled(GuardPawn->bEnableHearing);



	// 경비 스탯 초기화
	// -------------------------------------------------------------------------------------------------------
	// BT/Blackboard 를 건드리기 전에 먼저 적용한다 - PatrolArrivalRadius/HeadGaugeUpdateInterval
	// 등이 아래에서 바로 쓰인다 (SelectNextPatrolPoint, 헤드 게이지 타이머 등록).
	ApplyGuardStats(InPawn);



	// Behavior Tree / Blackboard 준비
	// -------------------------------------------------------------------------------------------------------

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



	// Perception 이벤트, PerceptionMeter 연결
	// -------------------------------------------------------------------------------------------------------
	PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AGuardAIController::OnTargetPerceptionUpdated);

	// 빙의한 폰의 PerceptionMeterComponent(소음 인지 게이지)를 찾아 OnPerceptionFull 을 구독한다.
	// 멤버 PerceptionMeter 에도 캐싱해 둬야 한다 - HandlePerceptionFull 에서 게이지를
	// 리셋(ResetPerception)할 때 이 멤버를 쓰는데, 로컬 변수에만 대입하고 멤버 대입을
	// 빠뜨리면 항상 nullptr 이라 리셋이 절대 호출되지 않는다. 그러면 래치가 안 풀려
	// 게이지가 100%에서 그대로 굳어 두 번째 소음부터는 OnPerceptionFull 이 다시 터지지 않는다.
	if (GuardPawn)
	{
		PerceptionMeter = GuardPawn->FindComponentByClass<UPerceptionMeterComponent>();
		if (PerceptionMeter)
		{
			PerceptionMeter->OnPerceptionFull.AddDynamic(this, &AGuardAIController::HandlePerceptionFull);
			//PerceptionMeter->OnPerceptionChanged.AddDynamic(this, &AGuardAIController::HandlePerceptionChanged);
		}
		else
		{
			UE_LOG(LogGuardAI, Error, TEXT("[%s] PerceptionMeterComponent 를 찾지 못했다. GuardCharacter 파생 폰인지 확인할 것."),
				*GetNameSafe(InPawn));
		}
	}


	// 머리 위 감지 게이지 타이머 등록
	// -------------------------------------------------------------------------------------------------------

	/// 	// 가드 캐릭터로 이동. 작동시 삭제
	/// // 머리 위 게이지 위젯도 BTService_UpdateDetectionGauge와 같은 주기로 갱신한다.
	/// // BT 서비스 쪽에 얹지 않고 별도 타이머로 두는 이유: BTService는 활성 브랜치에서만
	/// // 도는데, 게이지 표시는 브랜치와 무관하게(순찰 중이라도 시야에 들어오면) 항상 필요하다.
	/// GetWorldTimerManager().SetTimer(HeadGaugeUpdateTimerHandle, this,
	/// 	&AGuardAIController::UpdateHeadGaugeWidget, HeadGaugeUpdateInterval, true);




	// 첫 순찰 지점 선택
	// -------------------------------------------------------------------------------------------------------
	// 시작 시 첫 순찰 지점을 미리 채워둔다
	// SelectNextPatrolPoint(); 	// 이동 필요
	// 아래 함수로 변경했음
	SelectNextAction(EGuardAIState::Patrol);


	// Behavior Tree 시작
	// -------------------------------------------------------------------------------------------------------
	// BP_GuardAIController 는 data only 블루프린트라 그래프에서 대신 호출할 곳이 없고,
	// bStartAILogicOnPossess 도 BrainComponent 가 있어야 의미가 있다(그 컴포넌트를
	// 만들어주는 게 바로 이 호출이다). 여기서 부르지 않으면 BT 가 아예 시작되지 않는다.
	RunBehaviorTree(BehaviorTreeAsset);
}

void AGuardAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromGameState();

	if (UWorld* World = GetWorld())
	{
		if (GameStateSetHandle.IsValid())
		{
			World->GameStateSetEvent.Remove(GameStateSetHandle);
			GameStateSetHandle.Reset();
		}
	}

	Super::EndPlay(EndPlayReason);
}




void AGuardAIController::BindToGameState(AGameStateBase* GameState)
{
	AHeistGameState* HeistState = Cast<AHeistGameState>(GameState);

	// 작업 레벨이 아니면(GuardTest · L_NoiseTest 등) 아무것도 하지 않는다 —
	// 그 맵들에는 페이즈가 없고, 경비는 지금처럼 계속 순찰해야 검증이 된다.
	//
	// 같은 GameState 로 두 번 불리는 것도 정상 경로다 (BeginPlay 와 GameStateSetEvent 가 겹친다).
	// 걸러내지 않으면 델리게이트가 두 번 붙는다
	if (!HeistState || HeistState == BoundGameState)
	{
		return;
	}

	UnbindFromGameState();

	BoundGameState = HeistState;
	HeistState->OnPhaseChanged.AddDynamic(this, &AGuardAIController::HandleHeistPhaseChanged);

	// 판이 이미 끝난 뒤에 스폰된 경비는 지나간 전환 알림을 못 받는다.
	// 지금 값으로 한 번 맞춰 두지 않으면 결과 화면 뒤에서 혼자 순찰을 시작한다
	const FGameplayTag CurrentPhase = HeistState->GetCurrentPhase();
	if (CurrentPhase.IsValid())
	{
		HandleHeistPhaseChanged(CurrentPhase, FGameplayTag(), HeistState->GetPhaseReason());
	}
}

void AGuardAIController::UnbindFromGameState()
{
	if (!BoundGameState)
	{
		return;
	}

	BoundGameState->OnPhaseChanged.RemoveDynamic(this, &AGuardAIController::HandleHeistPhaseChanged);
	BoundGameState = nullptr;
}



void AGuardAIController::HandleHeistPhaseChanged(
	FGameplayTag NewPhase, FGameplayTag /*OldPhase*/, EHeistPhaseReason /*Reason*/)
{
	if (NewPhase.MatchesTag(HHTags::Phase_Result))
	{
		StopForMatchEnd();
	}
}

void AGuardAIController::StopForMatchEnd()
{
	UE_LOG(LogGuardAI, Log, TEXT("[%s] 판이 끝나 순찰을 멈춘다."), *GetNameSafe(GetPawn()));

	// BT 를 먼저 세운다. 이동 정지보다 나중에 하면 정지 직후 태스크가 한 번 더 돌아
	// 새 목적지를 잡아 버릴 수 있다
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Heist finished"));
	}

	StopMovement();

	// 지각도 끈다. 안 끄면 결과 화면이 떠 있는 동안에도 인지 게이지가 차고 경계도가 계속 오른다 —
	// 화면에는 멈춰 선 경비가 보이는데 숫자만 움직이는 상태가 된다
	if (PerceptionComp)
	{
		// 순찰도 꺼야할지

		GuardSightComp->SetSightEnabled(false);
		GuardHearingComp->SetHearingEnabled(false);
	}

	// 가드 캐릭터로 이동. 작동시 삭제
	/// // 머리 위 게이지 갱신 타이머도 멈춘다. 게이지는 더 이상 변하지 않는다
	/// GetWorldTimerManager().ClearTimer(HeadGaugeUpdateTimerHandle);

	if (AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(GetPawn()))
	{
		GuardPawn->StopHeadGaugeUpdate();
	}

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


	// if였던 것은 sight를 우선순위로 처리 (BP)
	GuardSightComp->OnTargetPerceptionUpdatedSight(Actor, Stimulus, BlackboardComp);
	GuardHearingComp->OnTargetPerceptionUpdatedHearing(Actor, Stimulus, BlackboardComp);

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

	// 세계 경계도(UAlertComponent)는 이 신호가 일정 횟수 쌓이면 병력을 증원한다.
	// ReportPursuitStarted() 와는 별개 카운터라 추격 횟수와 섞이지 않는다.
	if (UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		Alert->ReportNoiseDetected();
	}

	// 리셋하지 않으면 래치가 풀리지 않아 경비가 영원히 100%에 박힌다 (PerceptionMeterComponent.h 참고)
	if (PerceptionMeter)
	{
		PerceptionMeter->ResetPerception();
	}
}



void AGuardAIController::ApplyGuardStats(APawn* InPawn)
{
	const UGuardSettings* Settings = UGuardSettings::Get();
	const UDataTable* StatsTable = Settings->GuardStats.LoadSynchronous();
	if (!IsValid(StatsTable))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] Project Settings > Guard > Guard Stats 가 비어 있다. 폴백값을 그대로 쓴다."),
			*GetNameSafe(InPawn));
		return;
	}

	// RowName == EGuardType 의 짧은 이름 문자열. UEnum::GetNameStringByValue 는
	// "EGuardType::Standard" 처럼 열거형 이름까지 붙어 나와 DataTable RowName 관례와
	// 어긋나므로, 여기서는 명시적으로 매핑한다.
	FName RowName;
	switch (GuardType)
	{
	case EGuardType::Standard: RowName = TEXT("Standard"); break;
	case EGuardType::Dog:      RowName = TEXT("Dog");      break;
	case EGuardType::Armed:    RowName = TEXT("Armed");    break;
	default:                   RowName = TEXT("Standard"); break;
	}

	const FGuardStatsRow* Row = StatsTable->FindRow<FGuardStatsRow>(RowName, TEXT("AGuardAIController::ApplyGuardStats"));
	if (!Row)
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] DT_GuardStats 에 행 '%s' 가 없다. 폴백값을 그대로 쓴다."),
			*GetNameSafe(InPawn), *RowName.ToString());
		return;
	}


	GuardPatrolComp->SetPatrolStats(Row->PatrolArrivalRadius, Row->SearchSweepCount, Row->SearchSweepRadius);

	GuardSightComp->SetSightConfig(Row->SightRadius, Row->LoseSightRadius, Row->PeripheralVisionAngleDegrees);
	GuardHearingComp->SetHearingRange(Row->HearingRange);


	AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(InPawn);
	if (!GuardPawn)
	{
		// 캐스팅 실패 로그
		return;
	}

	// 가드 캐릭터로 이동. 작동시 삭제
	//HeadGaugeUpdateInterval = Row->HeadGaugeUpdateInterval;
	GuardPawn->SetHeadGaugeUpdateInterval(Row->HeadGaugeUpdateInterval);

	// 반경/각도를 런타임에 바꿨으니 Perception 시스템에 다시 알려야 실제 감지에 반영된다.
	PerceptionComp->RequestStimuliListenerUpdate();

	if (UCharacterMovementComponent* MovementComp = GuardPawn->GetCharacterMovement())
	{
		MovementComp->MaxWalkSpeed = Row->MoveSpeed;
	}
	

	// PerceptionMeter 멤버는 이 시점에 아직 캐싱되지 않았다(OnPossess 에서 이 함수보다
	// 뒤에 찾는다) - 여기서는 InPawn 에서 직접 다시 찾는다.
	if (UPerceptionMeterComponent* Meter = GuardPawn->FindComponentByClass<UPerceptionMeterComponent>())
	{
		Meter->SetDecayRate(Row->PerceptionDecayPerSecond);
	}

}


bool AGuardAIController::SelectNextAction(EGuardAIState State)
{

	if (!IsValid(GuardPatrolComp))
	{
		// 에러 로그
		return false;
	}

	SetAIState(State);

	switch (State)
	{
	case EGuardAIState::Patrol:
		GuardPatrolComp->SelectNextPatrolPoint2();
		return true;

	case EGuardAIState::Search:
		return GuardPatrolComp->SelectNextSearchPoint2();

	case EGuardAIState::Chase:
		return true;
	}

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

float AGuardAIController::GetDetectionGaugePercent() const
{
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	return IsValid(BlackboardComp) ? BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge) : 0.f;
}

bool AGuardAIController::IsTargeting(const AActor* InActor) const
{
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!IsValid(BlackboardComp) || !IsValid(InActor))
	{
		return false;
	}

	return BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor) == InActor;
}

/* 	// 가드 캐릭터로 이동. 작동시 삭제
void AGuardAIController::UpdateHeadGaugeWidget()
{
	AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(GetPawn());
	if (!IsValid(GuardPawn))
	{
		return;
	}

	UWidgetComponent* WidgetComp = GuardPawn->GetDetectionGaugeWidgetComponent();
	if (!IsValid(WidgetComp))
	{
		return;
	}

	UDetectionGaugeWidget* GaugeWidget = Cast<UDetectionGaugeWidget>(WidgetComp->GetUserWidgetObject());
	if (!GaugeWidget)
	{
		// Widget Class 가 아직 지정 안 됐거나(파생 BP에서 WBP_DetectionGauge 미설정),
		// 컴포넌트가 아직 위젯 인스턴스를 만들기 전(BeginPlay 타이밍)일 수 있다.
		return;
	}

	// 인덱스 0 로컬 플레이어 기준. 이 프로토타입은 단일 플레이어 대상 테스트 씬이라
	// 화면 하나에 여러 로컬 플레이어가 동시에 있는 상황(스플릿스크린)은 다루지 않는다.
	const APawn* LocalPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const float GaugePercent = IsTargeting(LocalPlayerPawn) ? GetDetectionGaugePercent() : 0.f;

	GaugeWidget->SetGaugePercent(GaugePercent);
}

*/
