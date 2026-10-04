
// 헤더 정리 0907
// Guard AI
#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"
#include "AI/GuardSettings.h"
#include "Engine/Engine.h"

#include "AI/GuardSightAComponent.h"
#include "AI/GuardHearingAComponent.h"
#include "AI/GuardPatrolAComponent.h"


// Guard Character
#include "Character/GuardCharacter.h"

// Behavior Tree / Blackboard
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"

// AI Perception
#include "AITypes.h"
#include "Perception/AIPerceptionComponent.h"

// Navigation
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"

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
#include "Components/CapsuleComponent.h"

// World / Actor
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

// UI
//#include "Components/WidgetComponent.h"
//#include "UI/DetectionGaugeWidget.h"
//#include "Kismet/GameplayStatics.h"





DEFINE_LOG_CATEGORY(LogGuardAI);



// 1. 생성자
AGuardAIController::AGuardAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

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

void AGuardAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (!HasAuthority() || !IsValid(PossessGuardPawn) || !PossessGuardPawn->IsDrawSightDebugEnabled() || !PossessGuardPawn->bDrawMoveTargetDebug)
	{
		return;
	}

	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	UWorld* World = GetWorld();
	if (!IsValid(BlackboardComp) || !IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// 표시 위치만 올리고 전경에 그려 시야 메시와 바닥에 가려지지 않도록 한다.
	const FVector DebugHeightOffset(0.0f, 0.0f, 200.0f);
	const FVector GuardLocation = PossessGuardPawn->GetActorLocation() + DebugHeightOffset;
	const AActor* AggroTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));
	if (IsValid(AggroTarget))
	{
		// 어그로 대상은 시야 여부와 이동 목적지에 관계없이 현재 위치를 표시한다.
		const FVector AggroTargetLocation = AggroTarget->GetActorLocation() + DebugHeightOffset;
		DrawDebugLine(World, GuardLocation, AggroTargetLocation, FColor::White, false, -1.0f, SDPG_Foreground, 4.0f);
		DrawDebugSphere(World, AggroTargetLocation, 30.0f, 12, FColor::Magenta, false, -1.0f, SDPG_Foreground, 2.0f);
	}

	FName TargetKey;
	FColor LineColor;
	FVector TargetLocation;
	switch (AIState)
	{
	case EGuardAIState::Patrol:
		TargetKey = GuardAIKeys::PatrolLocation;
		LineColor = FColor::Green;
		break;
	case EGuardAIState::Search:
		TargetKey = GuardAIKeys::InvestigateLocation;
		LineColor = FColor::Yellow;
		break;
	case EGuardAIState::Chase:
		LineColor = FColor::Red;
		if (BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget))
		{
			const AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));
			if (IsValid(TargetActor))
			{
				TargetLocation = TargetActor->GetActorLocation();
				break;
			}
		}
		TargetKey = GuardAIKeys::LastKnownLocation;
		break;
	default:
		return;
	}

	if (!TargetKey.IsNone())
	{
		if (!BlackboardComp->IsVectorValueSet(TargetKey))
		{
			return;
		}
		TargetLocation = BlackboardComp->GetValueAsVector(TargetKey);
	}

	if (!FAISystem::IsValidLocation(TargetLocation))
	{
		return;
	}

	// 매 프레임 다시 그려 목표 이동과 옵션 해제가 즉시 표시되도록 한다.
	const FVector DebugTargetLocation = TargetLocation + DebugHeightOffset;
	DrawDebugLine(World, GuardLocation, DebugTargetLocation, FColor::White, false, -1.0f, SDPG_Foreground, 2.0f);
	// 어그로 대상 마커와 겹쳐도 구분할 수 있도록 이동 목표 구체를 더 크게 표시한다.
	DrawDebugSphere(World, DebugTargetLocation, 45.0f, 16, LineColor, false, -1.0f, SDPG_Foreground, 3.0f);
#endif
}

bool AGuardAIController::IsVisibleChaseMove(const FAIMoveRequest& MoveRequest) const
{
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	return MoveRequest.IsMoveToActorRequest() && MoveRequest.IsUsingPathfinding() && IsValid(MoveRequest.GetGoalActor()) && IsValid(BlackboardComp) && BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget) && BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge) >= 100.0f && BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor) == MoveRequest.GetGoalActor();
}

FPathFollowingRequestResult AGuardAIController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	if (!HasAuthority())
	{
		return FPathFollowingRequestResult();
	}

	// 완료 콜백에서 BT가 다른 분기로 이동할 수 있으므로 요청 당시 조건을 보관한다.
	const bool bVisibleChaseMove = IsVisibleChaseMove(MoveRequest);
	if (bVisibleChaseMove)
	{
		LastChaseMoveRequestDebug = FString::Printf(TEXT("이동허용거리=%.1fcm 경비반경포함=%s 타겟반경포함=%s"), MoveRequest.GetAcceptanceRadius(), MoveRequest.IsReachTestIncludingAgentRadius() ? TEXT("예") : TEXT("아니오"), MoveRequest.IsReachTestIncludingGoalRadius() ? TEXT("예") : TEXT("아니오"));
		LastChasePathDebug = TEXT("최근경로=미확인");
	}
	const FPathFollowingRequestResult Result = Super::MoveTo(MoveRequest, OutPath);
	if (bVisibleChaseMove && Result.Code == EPathFollowingRequestResult::Failed)
	{
		const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		LogSearchTransitionDebug(TEXT("ChaseMoveRequestFailed"), FString::Printf(TEXT("추격 이동 요청 실패: Request=%u Pawn=%s PathFollowing=%s Navigation=%s Start=%s %s"), Result.MoveId.GetID(), *GetNameSafe(GetPawn()), *GetNameSafe(GetPathFollowingComponent()), *GetNameSafe(NavSys), *GetNavAgentLocation().ToCompactString(), *MoveRequest.ToString()));
	}
	return Result;
}

void AGuardAIController::FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const
{
	if (!HasAuthority())
	{
		return;
	}
	if (!IsVisibleChaseMove(MoveRequest))
	{
		Super::FindPathForMoveRequest(MoveRequest, Query, OutPath);
		return;
	}

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	const ANavigationData* NavData = Query.NavData.Get();
	if (!IsValid(NavSys) || !IsValid(NavData))
	{
		LogSearchTransitionDebug(TEXT("ChaseNavigationMissing"), TEXT("추격 경로의 NavigationSystem 또는 NavData 없음"));
		return;
	}

	// Actor 요청에는 엔진 MoveTo의 위치 목표 투영이 적용되지 않는다. 목표 Actor는 그대로 두고 경로의 끝점만 보정한다.
	const FVector OriginalGoal = Query.EndLocation;
	FVector ProjectionExtent = NavData->GetDefaultQueryExtent();
	const UCapsuleComponent* Capsule = IsValid(PossessGuardPawn) ? PossessGuardPawn->GetCapsuleComponent() : nullptr;
	if (IsValid(Capsule))
	{
		ProjectionExtent.Z = FMath::Max(ProjectionExtent.Z, static_cast<FVector::FReal>(Capsule->GetScaledCapsuleHalfHeight() * 2.0f));
	}
	FNavLocation ProjectedGoal;
	const bool bProjected = MoveRequest.IsProjectingGoal() && NavSys->ProjectPointToNavigation(OriginalGoal, ProjectedGoal, ProjectionExtent, NavData, Query.QueryFilter);
	if (bProjected)
	{
		Query.EndLocation = ProjectedGoal.Location;
	}

	// 이 값은 Path의 QueryData에도 저장된다. Actor 이동에 따른 엔진 재탐색에서도 NavMesh 밖 목표를 즉시 실패시키지 않는다.
	// BT/AITask가 원래 허용한 부분 경로만 사용한다. Allow Partial Path가 꺼져 있으면 완전 경로 요구를 보존한다.
	if (MoveRequest.IsUsingPartialPaths())
	{
		Query.SetRequireNavigableEndLocation(false);
	}

	FPathFindingResult PathResult = NavSys->FindPathSync(Query);
	if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid())
	{
		LogSearchTransitionDebug(TEXT("ChasePathFailed"), FString::Printf(TEXT("추격 경로 생성 실패: Result=%d NavData=%s Start=%s Original=%s QueryGoal=%s Projected=%d Extent=%s AllowPartial=%d"), static_cast<int32>(PathResult.Result), *GetNameSafe(NavData), *Query.StartLocation.ToCompactString(), *OriginalGoal.ToCompactString(), *Query.EndLocation.ToCompactString(), bProjected, *ProjectionExtent.ToCompactString(), MoveRequest.IsUsingPartialPaths()));
		return;
	}

	// 수평 보정으로 목표와 다른 지점에 도착하는 경로는 부분 경로다. 마지막 구간에서 Actor 원위치로 직진하는 것을 막는다.
	if (bProjected && MoveRequest.IsUsingPartialPaths() && FVector::DistSquared2D(OriginalGoal, ProjectedGoal.Location) > FMath::Square(1.0f))
	{
		PathResult.Path->SetIsPartial(true);
	}
	PathResult.Path->SetGoalActorObservation(*MoveRequest.GetGoalActor(), 100.0f);
	PathResult.Path->EnableRecalculationOnInvalidation(true);
	// 엔진의 Actor 재탐색은 이 함수를 거치지 않는다. 재탐색 뒤에도 보정된 끝점과 Actor가 다르면 부분 경로로 유지한다.
	PathResult.Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &AGuardAIController::HandleChasePathUpdated));
	OutPath = PathResult.Path;
	LastChasePathDebug = FString::Printf(TEXT("최근경로부분=%s 경로끝=%s"), OutPath->IsPartial() ? TEXT("예") : TEXT("아니오"), *OutPath->GetEndLocation().ToCompactString());
	LogSearchTransitionDebug(TEXT("ChasePathReady"), FString::Printf(TEXT("추격 Actor 추적 유지: NavData=%s Start=%s Original=%s QueryGoal=%s PathEnd=%s Projected=%d Partial=%d AllowPartial=%d"), *GetNameSafe(NavData), *Query.StartLocation.ToCompactString(), *OriginalGoal.ToCompactString(), *Query.EndLocation.ToCompactString(), *OutPath->GetEndLocation().ToCompactString(), bProjected, OutPath->IsPartial(), MoveRequest.IsUsingPartialPaths()));
}

void AGuardAIController::HandleChasePathUpdated(FNavigationPath* UpdatedPath, ENavPathEvent::Type Event) const
{
	if (!HasAuthority() || !UpdatedPath || (Event != ENavPathEvent::UpdatedDueToGoalMoved && Event != ENavPathEvent::UpdatedDueToNavigationChanged))
	{
		return;
	}
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	const AActor* GoalActor = UpdatedPath->GetGoalActor();
	if (!IsValid(GoalActor) || !IsValid(BlackboardComp) || !BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget) || BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor) != GoalActor || !UpdatedPath->GetQueryData().bAllowPartialPaths)
	{
		return;
	}
	// 저장된 QueryData의 끝점은 최초 요청 값일 수 있으므로 현재 관찰 중인 Actor의 목표를 비교한다.
	const FVector ActorGoal = UpdatedPath->GetGoalLocation();
	const FVector PathEnd = UpdatedPath->GetEndLocation();
	if (FVector::DistSquared2D(ActorGoal, PathEnd) > FMath::Square(1.0f))
	{
		UpdatedPath->SetIsPartial(true);
	}
	LastChasePathDebug = FString::Printf(TEXT("최근경로부분=%s 경로끝=%s"), UpdatedPath->IsPartial() ? TEXT("예") : TEXT("아니오"), *PathEnd.ToCompactString());
	LogSearchTransitionDebug(TEXT("ChasePathUpdated"), FString::Printf(TEXT("추격 경로 재탐색: ActorGoal=%s PathEnd=%s Partial=%d"), *ActorGoal.ToCompactString(), *PathEnd.ToCompactString(), UpdatedPath->IsPartial()));
}

void AGuardAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (HasAuthority())
	{
		const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
		const bool bHasFullTarget = IsValid(BlackboardComp) && BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge) >= 100.0f && IsValid(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));
		if (AIState == EGuardAIState::Chase || bHasFullTarget)
		{
			// 정상 완료, 경로 실패, 다른 요청에 의한 중단을 구분한다. 상태값만으로 실제 BT 분기를 단정하지 않는다.
			const FName Event(*FString::Printf(TEXT("ChaseMoveCompleted_%d_%u"), static_cast<int32>(Result.Code.GetValue()), static_cast<uint32>(Result.Flags)));
			LogSearchTransitionDebug(Event, FString::Printf(TEXT("Request=%u Result=%s Code=%d Flags=%u Success=%d Interrupted=%d LastKnown=%s"), RequestID.GetID(), *Result.ToString(), static_cast<int32>(Result.Code.GetValue()), static_cast<uint32>(Result.Flags), Result.IsSuccess(), Result.IsInterrupted(), IsValid(BlackboardComp) ? *BlackboardComp->GetValueAsVector(GuardAIKeys::LastKnownLocation).ToCompactString() : TEXT("NoBlackboard")));
			if (Result.IsSuccess())
			{
				LogArrestRangeDebug(LastArrestRangeDebug, true);
			}
		}
	}

	// BT의 완료 통지 전에 실패를 기록해 다음 지점 선택에 반영한다.
	if (HasAuthority() && AIState == EGuardAIState::Patrol && IsValid(GuardPatrolComp) && Result.IsFailure() && !Result.IsInterrupted())
	{
		GuardPatrolComp->SkipCurrentPatrolPoint();
	}
	Super::OnMoveCompleted(RequestID, Result);
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
	if (!HasAuthority() || bMatchEnded)
	{
		return;
	}

	PossessGuardPawn = Cast<AGuardCharacter>(InPawn);
	if (!IsValid(PossessGuardPawn))
	{
		UE_LOG(LogGuardAI, Error, TEXT("[%s] GuardPawn이 유효하지 않습니다. 현재 빙의한 Pawn=%s"), *GetNameSafe(this), *GetNameSafe(GetPawn()));
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("[GuardAI] 빙의한 GuardPawn을 가져오지 못했습니다."));
		return;
	}

	// --------------------------------------------------------------------------------------------

	GuardSightComp->Initialize(PossessGuardPawn, PerceptionComp);
	GuardHearingComp->Initialize(PossessGuardPawn, PerceptionComp);

	// 경비 스탯 초기화
	// -------------------------------------------------------------------------------------------------------
	// BT/Blackboard 를 건드리기 전에 먼저 적용한다 - PatrolArrivalRadius/HeadGaugeUpdateInterval
	// 등이 아래에서 바로 쓰인다 (SelectNextPatrolPoint, 헤드 게이지 타이머 등록).
	ApplyGuardStats();


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


	//PossessGuardPawn
	if (PossessGuardPawn->GetPerceptionMeterComponent())
	{
		PossessGuardPawn->GetPerceptionMeterComponent()->OnPerceptionFull.AddDynamic(this, &AGuardAIController::HandlePerceptionFull);
	}

	// ?
	/*
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
	*/

	// 첫 순찰 지점 선택 : 시작 시 첫 순찰 지점을 미리 채워둔다
	// -------------------------------------------------------------------------------------------------------
	// SelectNextPatrolPoint();-> 아래 함수로 변경했음
	bWorldAlarmBehaviorActive = false;
	BindToWorldAlert();
	if (IsWorldAlarmActive())
	{
		UpdateWorldAlarmBehavior();
	}
	else
	{
		SelectNextAction(EGuardAIState::Patrol);
	}


	// Behavior Tree 시작
	// -------------------------------------------------------------------------------------------------------
	// BP_GuardAIController 는 data only 블루프린트라 그래프에서 대신 호출할 곳이 없고,
	// bStartAILogicOnPossess 도 BrainComponent 가 있어야 의미가 있다(그 컴포넌트를
	// 만들어주는 게 바로 이 호출이다). 여기서 부르지 않으면 BT 가 아예 시작되지 않는다.
	RunBehaviorTree(BehaviorTreeAsset);
	UpdateMoveSpeedByWorldAlert(GetWorldAlertLevel() / 100.f);
}

void AGuardAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromGameState();
	if (IsValid(BoundAlertComponent))
	{
		BoundAlertComponent->OnAlertGaugeChanged.RemoveDynamic(this, &AGuardAIController::UpdateMoveSpeedByWorldAlert);
		BoundAlertComponent->OnAlertLevelChanged.RemoveDynamic(this, &AGuardAIController::HandleWorldAlertLevelChanged);
		BoundAlertComponent = nullptr;
	}

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
	BindToWorldAlert();
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



void AGuardAIController::ResetMoveSpeed()
{
	if (!HasAuthority() || !IsValid(PossessGuardPawn))
	{
		return;
	}
	if (IsWorldAlarmActive())
	{
		ApplyCurrentMoveSpeed();
		return;
	}

	PossessGuardPawn->GetCharacterMovement()->MaxWalkSpeed = GetNormalMoveSpeed();
	SetWorldAlertSpeedUp(false);

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
	if (!HasAuthority())
	{
		return;
	}
	bMatchEnded = true;
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


	if (PossessGuardPawn)
	{
		PossessGuardPawn->StopHeadGaugeUpdate();
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
	UPerceptionMeterComponent* PerceptionMeter = IsValid(PossessGuardPawn) ? PossessGuardPawn->GetPerceptionMeterComponent() : nullptr;
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	const bool bVisibleChaseTarget = IsValid(BlackboardComp) && BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget) && IsValid(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor)) && GetDetectionGaugePercent() >= 100.f;
	if (!IsValid(BlackboardComp) || AIState == EGuardAIState::Chase || bVisibleChaseTarget)
	{
		if (IsValid(PerceptionMeter))
		{
			PerceptionMeter->ResetPerception();
		}
		return;
	}

	//+ 0910 디버그용
	UE_LOG(LogTemp, Warning, TEXT("[GuardAI] PerceptionFull RECEIVED | Location=%s"), *LastNoiseLocation.ToString());

	RequestInvestigate(LastNoiseLocation);

	// 평온에서는 기존처럼 즉시 비운다. 의심 이상에서 강제로 채운 게이지는
	// 조사 중 유지하고 순찰 복귀·추격 전환 때 해제한다.
	if (IsValid(PerceptionMeter) && !PerceptionMeter->IsHoldingAlertInvestigationGauge())
	{
		PerceptionMeter->ResetPerception();
	}
}

void AGuardAIController::RequestInvestigate(FVector Location)
{
	// 여러 시스템(청각 게이지, 카메라 등)이 부를 수 있는 공개 API 라 게이트를 안에 둔다
	// (CLAUDE.md 3절 "여러 사람이 호출하는 API 는 게이트를 API 안에 둔다" 규칙)
	if (!HasAuthority())
	{
		return;
	}

	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, Location);
		BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, GetWorld()->GetTimeSeconds());
	}

	// 세계 경계도(UAlertComponent)는 이 신호가 일정 횟수 쌓이면 병력을 증원한다.
	// ReportPursuitStarted() 와는 별개 카운터라 추격 횟수와 섞이지 않는다.
	if (UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		Alert->ReportNoiseDetected();
	}
}





void AGuardAIController::ApplyGuardStats()// APawn* InPawn)
{

	if (!PossessGuardPawn)
	{
		return;
	}

	const UGuardSettings* Settings = UGuardSettings::Get();
	const UDataTable* StatsTable = Settings->GuardStats.LoadSynchronous();
	if (!IsValid(StatsTable))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] Project Settings > Guard > Guard Stats 가 비어 있다. 폴백값을 그대로 쓴다."),
			*GetNameSafe(PossessGuardPawn));
		return;
	}

	// RowName == EGuardType 의 짧은 이름 문자열. UEnum::GetNameStringByValue 는
	// "EGuardType::Standard" 처럼 열거형 이름까지 붙어 나와 DataTable RowName 관례와
	// 어긋나므로, 여기서는 명시적으로 매핑한다.
	FName RowName;
	switch (PossessGuardPawn->GuardType)
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
			*GetNameSafe(PossessGuardPawn), *RowName.ToString());
		return;
	}



	// DataTable에서 설정한 이동 속도를 기본 속도로 저장한다.
	SetNormalMoveSpeed(Row->MoveSpeed);
	SetChaseMoveSpeed(Row->MoveSpeedChase);
	PossessGuardPawn->SetGuardMoveSpeed(GetNormalMoveSpeed());


	PossessGuardPawn->SetHeadGaugeUpdateInterval(Row->HeadGaugeUpdateInterval);

	GuardPatrolComp->SetPatrolStats(Row->PatrolArrivalRadius, Row->SearchSweepCount, Row->SearchSweepRadius);

	GuardSightComp->SetSightConfig(Row->SightRadius, Row->LoseSightRadius,
		Row->PeripheralVisionAngleDegrees, Row->VerticalVisionAngleDegrees, Row->BinocularVisionAngleDegrees);

	GuardHearingComp->SetHearingRange(Row->HearingRange);
	UE_LOG(LogGuardAI, Warning, TEXT("[%s] ApplyGuardStats HearingRange = %.1f, Actual = %.1f"),
		*GetName(), Row->HearingRange, GuardHearingComp->GetHearingRange());

	// 반경/각도를 런타임에 바꿨으니 Perception 시스템에 다시 알려야 실제 감지에 반영된다.
	PerceptionComp->RequestStimuliListenerUpdate();

	

	// PerceptionMeter 멤버는 이 시점에 아직 캐싱되지 않았다(OnPossess 에서 이 함수보다
	// 뒤에 찾는다) - 여기서는 InPawn 에서 직접 다시 찾는다.
	if (UPerceptionMeterComponent* Meter = PossessGuardPawn->FindComponentByClass<UPerceptionMeterComponent>())
	{
		Meter->SetDecayRate(Row->PerceptionDecayPerSecond);
	}

}

FString AGuardAIController::GetPossessGuardPawnName() const
{
	return PossessGuardPawn ? PossessGuardPawn->GetName() : TEXT("None");
}

void AGuardAIController::SetSightDebugEnabled(bool bInEnabled)
{
	if (!GuardSightComp)
	{
		return;
	}

	GuardSightComp->SetSightDebugEnabled(bInEnabled);
}


void AGuardAIController::SetAIState(EGuardAIState NewState)
{
	if (!HasAuthority())
	{
		return;
	}
	// 경보 중에는 다른 호출 경로에서도 순찰 상태로 되돌릴 수 없다.
	if (NewState == EGuardAIState::Patrol && IsWorldAlarmActive())
	{
		NewState = EGuardAIState::Search;
	}
	if (AIState == NewState)
	{
		return;
	}

	AGuardCharacter* GuardPawn = PossessGuardPawn.Get();
	// 의심 단계에서 채운 청각 게이지는 조사 종료 또는 추격 전환 때 해제한다.
	if (HasAuthority() && IsValid(GuardPawn) && NewState != EGuardAIState::Search)
	{
		UPerceptionMeterComponent* PerceptionMeter = GuardPawn->GetPerceptionMeterComponent();
		if (IsValid(PerceptionMeter) && PerceptionMeter->IsHoldingAlertInvestigationGauge())
		{
			PerceptionMeter->ResetPerception();
		}
	}
	const FString PawnName = IsValid(GuardPawn) ? GuardPawn->GetName() : TEXT("InvalidPawn");

	const UEnum* GuardAIStateEnum = StaticEnum<EGuardAIState>();
	const FString PreviousStateName = AIState == EGuardAIState::Patrol ? TEXT("순찰") : AIState == EGuardAIState::Search ? TEXT("수색") : AIState == EGuardAIState::Chase ? TEXT("추격") : GuardAIStateEnum->GetNameStringByValue(static_cast<int64>(AIState));
	const FString NewStateName = NewState == EGuardAIState::Patrol ? TEXT("순찰") : NewState == EGuardAIState::Search ? TEXT("수색") : NewState == EGuardAIState::Chase ? TEXT("추격") : GuardAIStateEnum->GetNameStringByValue(static_cast<int64>(NewState));
	UE_LOG(LogGuardAI, Log, TEXT("[AI][%s] %s → %s"),
		*PawnName, *PreviousStateName, *NewStateName);

	AIState = NewState;
}

void AGuardAIController::LogArrestRangeDebug(float ArrestRange, bool bMoveCompleted) const
{
	if (!HasAuthority() || !IsValid(GetWorld()) || !IsValid(GetPawn()) || !IsValid(GetBlackboardComponent()))
	{
		return;
	}
	const AActor* TargetActor = Cast<AActor>(GetBlackboardComponent()->GetValueAsObject(GuardAIKeys::TargetActor));
	if (!IsValid(TargetActor))
	{
		return;
	}
	if (ArrestRange >= 0.f)
	{
		LastArrestRangeDebug = ArrestRange;
	}
	const FName Event = bMoveCompleted ? TEXT("ArrestDebugMoveCompleted") : TEXT("ArrestDebugCheck");
	const float Now = GetWorld()->GetTimeSeconds();
	const float* LastLogTime = SearchDebugLastLogTimes.Find(Event);
	if (LastLogTime && Now - *LastLogTime < 1.f)
	{
		return;
	}
	SearchDebugLastLogTimes.Add(Event, Now);
	const FVector GuardLocation = GetPawn()->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const float Distance = FVector::Dist(GuardLocation, TargetLocation);
	const float HorizontalDistance = FVector::Dist2D(GuardLocation, TargetLocation);
	float GuardRadius = 0.f;
	float GuardHalfHeight = 0.f;
	float TargetRadius = 0.f;
	float TargetHalfHeight = 0.f;
	GetPawn()->GetSimpleCollisionCylinder(GuardRadius, GuardHalfHeight);
	TargetActor->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);
	const FString Message = FString::Printf(TEXT("[체포 거리][%s][%s] 대상=%s 중심거리=%.1fcm 수평=%.1fcm 높이차=%.1fcm 체포범위=%.1fcm 판정=%s 반경(경비/타겟)=%.1f/%.1fcm | %s | %s"), *GetNameSafe(GetPawn()), bMoveCompleted ? TEXT("이동 성공 직후") : TEXT("조건 검사"), *GetNameSafe(TargetActor), Distance, HorizontalDistance, FMath::Abs(GuardLocation.Z - TargetLocation.Z), LastArrestRangeDebug, LastArrestRangeDebug < 0.f ? TEXT("범위 미확인") : Distance <= LastArrestRangeDebug ? TEXT("범위 안") : TEXT("범위 밖"), GuardRadius, TargetRadius, LastChaseMoveRequestDebug.IsEmpty() ? TEXT("최근 이동 요청 없음") : *LastChaseMoveRequestDebug, LastChasePathDebug.IsEmpty() ? TEXT("최근 경로 없음") : *LastChasePathDebug);
	UE_LOG(LogGuardAI, Warning, TEXT("%s"), *Message);
	if (IsValid(GEngine) && GetNetMode() != NM_DedicatedServer)
	{
		const uint64 MessageKey = (static_cast<uint64>(GetUniqueID()) << 32) | 2;
		GEngine->AddOnScreenDebugMessage(MessageKey, 2.f, Distance <= LastArrestRangeDebug ? FColor::Green : FColor::Red, Message);
	}
}

void AGuardAIController::LogSearchTransitionDebug(FName Event, const FString& Detail) const
{
	if (!HasAuthority())
	{
		return;
	}
	const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	const UWorld* World = GetWorld();
	if (!IsValid(BlackboardComp) || !IsValid(World))
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const FString EventName = Event.ToString();
	if (!bDetailedSightSearchLogs && (EventName == TEXT("SightGaugeSnapshot") || EventName.StartsWith(TEXT("Timeout_")) || EventName == TEXT("FinishRejected") || EventName == TEXT("ChasePathReady") || EventName == TEXT("ChasePathUpdated")))
	{
		return;
	}
	const float* LastLogTime = SearchDebugLastLogTimes.Find(Event);
	if (LastLogTime && Now - *LastLogTime < 1.0f)
	{
		return;
	}
	SearchDebugLastLogTimes.Add(Event, Now);

	const float SearchStartTime = BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime);
	const int32 SearchStep = IsValid(GuardPatrolComp) ? GuardPatrolComp->GetCurrentSearchStep() : INDEX_NONE;
	const int32 SweepCount = IsValid(GuardPatrolComp) ? GuardPatrolComp->SearchSweepCount : 0;
	if (!bDetailedSightSearchLogs)
	{
		static const TMap<FName, FString> EventLabels = {
			{TEXT("PatrolEntry"), TEXT("순찰 시작")},
			{TEXT("SearchEntry"), TEXT("수색 시작")},
			{TEXT("PatrolWhileTargetVisible"), TEXT("경고: 타겟이 보이는데 순찰 진입")},
			{TEXT("FinishStarted"), TEXT("마무리 수색 시작")},
			{TEXT("FinishMoveRequest"), TEXT("마무리 이동 요청")},
			{TEXT("FinishMoveRetry"), TEXT("마무리 이동 실패·재시도")},
			{TEXT("FinishInterrupted"), TEXT("재발견·새 수색으로 마무리 중단")},
			{TEXT("FinishCompleted"), TEXT("마무리 대기 완료")},
			{TEXT("FinishAbort"), TEXT("마무리 수색 중단")},
			{TEXT("SearchNavigationMissing"), TEXT("수색 경로 정보 없음")},
			{TEXT("SearchGoalProjected"), TEXT("수색 목적지 보정")},
			{TEXT("SearchAlternativeGoal"), TEXT("수색 대체 목적지 선택")},
			{TEXT("SearchNoReachableGoal"), TEXT("도달 가능한 수색 지점 없음")},
			{TEXT("SearchSelectFailed"), TEXT("수색 위치 무효")},
			{TEXT("SearchExhausted"), TEXT("수색 횟수 소진")},
			{TEXT("SearchPointFailed"), TEXT("수색 지점 선택 실패")},
			{TEXT("ChaseMoveRequestFailed"), TEXT("추격 이동 요청 실패")},
			{TEXT("ChaseNavigationMissing"), TEXT("추격 경로 정보 없음")},
			{TEXT("ChasePathFailed"), TEXT("추격 경로 생성 실패")},
			{TEXT("InvestigateInvalidGoal"), TEXT("수색 이동 목적지 무효")},
			{TEXT("InvestigateMoveRequest"), TEXT("수색 이동 요청")},
			{TEXT("InvestigateMoveRetry"), TEXT("수색 이동 실패·재시도")},
			{TEXT("InvestigateIdle"), TEXT("수색 이동 종료")},
			{TEXT("InvestigateAbort"), TEXT("수색 이동 중단")}
		};
		const FString* EventLabel = EventLabels.Find(Event);
		FString Summary = EventLabel ? *EventLabel : Detail;
		if (EventName.StartsWith(TEXT("ChaseMoveCompleted_")))
		{
			Summary = TEXT("추격 이동 종료");
			TArray<FString> Parts;
			EventName.ParseIntoArray(Parts, TEXT("_"));
			if (Parts.Num() >= 2)
			{
				const int32 ResultCode = FCString::Atoi(*Parts[1]);
				const TCHAR* ResultLabel = ResultCode == 0 ? TEXT("성공") : ResultCode == 1 ? TEXT("막힘") : ResultCode == 2 ? TEXT("경로 이탈") : ResultCode == 3 ? TEXT("중단") : ResultCode == 5 ? TEXT("무효") : TEXT("기타");
				Summary += FString::Printf(TEXT("(%s)"), ResultLabel);
			}
		}
		const TCHAR* StateLabel = AIState == EGuardAIState::Patrol ? TEXT("순찰") : AIState == EGuardAIState::Search ? TEXT("수색") : TEXT("추격");
		const float SearchElapsed = SearchStartTime >= 0.f ? FMath::Max(0.f, Now - SearchStartTime) : 0.f;
		const FString SearchInfo = AIState == EGuardAIState::Search ? FString::Printf(TEXT(" 경과=%.1f초 단계=%d/%d"), SearchElapsed, SearchStep, SweepCount) : TEXT("");
		const int32 GoalStart = Detail.Find(TEXT("Goal=V("));
		if (GoalStart != INDEX_NONE)
		{
			const int32 GoalEnd = Detail.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, GoalStart);
			if (GoalEnd != INDEX_NONE)
			{
				Summary += TEXT(" 목적지=") + Detail.Mid(GoalStart + 5, GoalEnd - GoalStart - 4);
			}
		}
		const int32 RequestStart = Detail.Find(TEXT("MoveRequest="));
		if (RequestStart != INDEX_NONE)
		{
			const int32 RequestResult = FCString::Atoi(*Detail.Mid(RequestStart + 12));
			Summary += RequestResult == 0 ? TEXT("(실패)") : RequestResult == 1 ? TEXT("(이미 도착)") : TEXT("(접수)");
		}
		UE_LOG(LogGuardAI, Log, TEXT("[AI][%s][%s] 상태=%s 시야=%s 게이지=%.0f 대상=%s%s"), *GetNameSafe(GetPawn()), *Summary, StateLabel, BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget) ? TEXT("보임") : TEXT("놓침"), BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge), *GetNameSafe(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor)), *SearchInfo);
		return;
	}
	UE_LOG(LogGuardAI, Warning, TEXT("[SearchDebug][%s][%s] %s | Now=%.3f State=%d See=%d Gauge=%.1f Start=%.3f Elapsed=%.3f LastSeenAge=%.3f Step=%d/%d MoveStatus=%d Target=%s Investigate=%s"), *GetNameSafe(GetPawn()), *Event.ToString(), *Detail, Now, static_cast<int32>(AIState), BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget), BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge), SearchStartTime, Now - SearchStartTime, Now - BlackboardComp->GetValueAsFloat(GuardAIKeys::LastSeenTime), SearchStep, SweepCount, static_cast<int32>(GetMoveStatus()), *GetNameSafe(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor)), *BlackboardComp->GetValueAsVector(GuardAIKeys::InvestigateLocation).ToCompactString());
}

bool AGuardAIController::SelectNextAction(EGuardAIState State)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!IsValid(GuardPatrolComp))
	{
		// 에러 로그
		UE_LOG(LogGuardAI, Error, TEXT("[%s] SelectNextAction 실패: GuardPatrolComp invalid"),
			*GetNameSafe(PossessGuardPawn));
		return false;
	}

	if (State == EGuardAIState::Patrol && IsWorldAlarmActive())
	{
		EnsureAlarmSearchSession();
		SetAIState(EGuardAIState::Search);
		LogSearchTransitionDebug(TEXT("AlarmPatrolBlocked"), TEXT("경보 중 순찰 요청 차단: 경보 수색 분기 설정 확인"));
		return false;
	}

	//if (AIState == State)
	//{
	//	return false;
	//}


	if (bDetailedSightSearchLogs)
	{
		const TCHAR* StateLabel = State == EGuardAIState::Patrol ? TEXT("순찰") : State == EGuardAIState::Search ? TEXT("수색") : TEXT("추격");
		UE_LOG(LogGuardAI, Log, TEXT("[AI][%s][행동 요청] %s"), *GetNameSafe(PossessGuardPawn), StateLabel);
	}

	if (State == EGuardAIState::Patrol)
	{
		const UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
		if (IsValid(BlackboardComp) && BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget) && BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge) >= 100.0f)
		{
			LogSearchTransitionDebug(TEXT("PatrolWhileTargetVisible"), FString::Printf(TEXT("대상이 보이고 게이지 100인데 순찰 태스크 실행: PreviousState=%d"), static_cast<int32>(AIState)));
		}
	}
	SetAIState(State);

	switch (State)
	{
	case EGuardAIState::Patrol:
		LogSearchTransitionDebug(TEXT("PatrolEntry"), TEXT("순찰 선택 태스크 실행"));

		if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
		{
			// 실제로 시야를 잃은 대상만 해제한다. 보이는 대상을 지우면 재감지 콜백 없이 UI가 0에 머물 수 있다.
			if (!BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget))
			{
				BlackboardComp->ClearValue(GuardAIKeys::TargetActor);
			}
		}

		return IsValid(GuardPatrolComp) && GuardPatrolComp->SelectNextPatrolPoint2();

	case EGuardAIState::Search:
		LogSearchTransitionDebug(TEXT("SearchEntry"), TEXT("수색 선택 태스크 실행"));
		return GuardPatrolComp->SelectNextSearchPoint2();

	//case EGuardAIState::Chase:
	//	UE_LOG(LogGuardAI, Warning, TEXT("[%s] Chase 전환 요청"),*GetNameSafe(PossessGuardPawn));
	//	return true;
	}

	return false;
}


void AGuardAIController::ApplyCurrentMoveSpeed()
{
	if (!HasAuthority())
	{
		return;
	}
	// 현재 경비의 이동 상태를 기준으로 최종 이동 속도를 계산하고 적용한다.
	// 월드 경계도에 의해 속도가 증가된 상태라면 WorldAlertMoveSpeedMultiplier를 추가로 적용한다.
	// 추격 상태와 월드 경계도 상태가 변경될 때만 호출한다.

	AGuardCharacter* GuardCharacter = GetPossessGuardPawn();
	if (!IsValid(GuardCharacter))
	{
		return;
	}

	UCharacterMovementComponent* MovementComp = GuardCharacter->GetCharacterMovement();
	if (!IsValid(MovementComp))
	{
		return;
	}

	// 경보 중에는 수색도 추격 속도로 이동한다.
	const bool bAlarm = IsWorldAlarmActive();
	float NewMoveSpeed = (bAlarm || WorldAlertSet.bIsChasing) ? WorldAlertSet.ChaseMoveSpeed : WorldAlertSet.NormalMoveSpeed;

	// 월드 경계도에 의해 속도가 증가된 상태라면 현재 선택된 이동 속도에 경계도 배율을 적용한다.
	if (bAlarm || WorldAlertSet.bWorldAlertSpeedUp)
	{
		NewMoveSpeed *= WorldAlertSet.WorldAlertMoveSpeedMultiplier;
	}

	// 계산된 최종 이동 속도를 경비의 CharacterMovement에 적용한다.
	MovementComp->MaxWalkSpeed = NewMoveSpeed;

	// 현재 적용된 이동 속도와 추격 및 월드 경계도 가속 상태를 확인할 수 있도록 로그를 출력한다.
	UE_LOG(LogGuardAI, Warning, TEXT("[%s] 이동속도 갱신: %.1f (Chasing=%d WorldAlertSpeedUp=%d)"),
		*GetNameSafe(GuardCharacter), NewMoveSpeed, WorldAlertSet.bIsChasing, WorldAlertSet.bWorldAlertSpeedUp);

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

bool AGuardAIController::IsWorldAlarmActive() const
{
	const UAlertComponent* Alert = UAlertComponent::Get(this);
	return IsValid(Alert) && Alert->IsAlarmed();
}

void AGuardAIController::BindToWorldAlert()
{
	if (!HasAuthority())
	{
		return;
	}
	UAlertComponent* Alert = UAlertComponent::Get(this);
	if (Alert == BoundAlertComponent.Get())
	{
		return;
	}
	if (IsValid(BoundAlertComponent))
	{
		BoundAlertComponent->OnAlertGaugeChanged.RemoveDynamic(this, &AGuardAIController::UpdateMoveSpeedByWorldAlert);
		BoundAlertComponent->OnAlertLevelChanged.RemoveDynamic(this, &AGuardAIController::HandleWorldAlertLevelChanged);
	}
	BoundAlertComponent = Alert;
	if (IsValid(Alert))
	{
		Alert->OnAlertGaugeChanged.AddUniqueDynamic(this, &AGuardAIController::UpdateMoveSpeedByWorldAlert);
		// 게이지 복제의 양자화 값이 이미 255여도 실제 경보 단계 진입은 별도로 받는다.
		Alert->OnAlertLevelChanged.AddUniqueDynamic(this, &AGuardAIController::HandleWorldAlertLevelChanged);
	}
}

void AGuardAIController::HandleWorldAlertLevelChanged(EAlertLevel NewLevel, EAlertLevel OldLevel)
{
	if (!HasAuthority() || (NewLevel != EAlertLevel::Alarm && OldLevel != EAlertLevel::Alarm))
	{
		return;
	}
	UpdateMoveSpeedByWorldAlert(GetWorldAlertLevel() / 100.f);
}

void AGuardAIController::EnsureAlarmSearchSession()
{
	if (!HasAuthority() || bMatchEnded || !IsWorldAlarmActive() || !IsValid(GetWorld()) || !IsValid(PossessGuardPawn))
	{
		return;
	}
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
	{
		return;
	}
	const bool bHasSearchLocation = BlackboardComp->IsVectorValueSet(GuardAIKeys::InvestigateLocation) && FAISystem::IsValidLocation(BlackboardComp->GetValueAsVector(GuardAIKeys::InvestigateLocation));
	if (!bHasSearchLocation)
	{
		FVector SearchLocation = PossessGuardPawn->GetActorLocation();
		if (BlackboardComp->IsVectorValueSet(GuardAIKeys::LastKnownLocation))
		{
			const FVector LastKnownLocation = BlackboardComp->GetValueAsVector(GuardAIKeys::LastKnownLocation);
			if (FAISystem::IsValidLocation(LastKnownLocation))
			{
				SearchLocation = LastKnownLocation;
			}
		}
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, SearchLocation);
	}
	if (!bHasSearchLocation || BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime) < 0.f)
	{
		// 새 소음 감지로 집계하지 않고 경보에 필요한 수색 세션만 만든다.
		BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, GetWorld()->GetTimeSeconds());
	}
}

void AGuardAIController::UpdateWorldAlarmBehavior()
{
	if (!HasAuthority() || bMatchEnded || !IsValid(PossessGuardPawn) || !IsValid(GetBlackboardComponent()))
	{
		return;
	}
	BindToWorldAlert();
	const bool bAlarm = IsWorldAlarmActive();
	const bool bAlarmChanged = bAlarm != bWorldAlarmBehaviorActive;
	bWorldAlarmBehaviorActive = bAlarm;
	if (bAlarm)
	{
		EnsureAlarmSearchSession();
	}
	if (!bAlarmChanged)
	{
		return;
	}
	if (IsValid(GuardHearingComp))
	{
		GuardHearingComp->ClearWorldAlertSilenceTimer();
	}
	WorldAlertSet.bWorldAlertSpeedTriggered = bAlarm;
	SetWorldAlertSpeedUp(bAlarm);
	ApplyCurrentMoveSpeed();
	UE_LOG(LogGuardAI, Log, TEXT("[AI][%s][경보] %s"), *GetNameSafe(PossessGuardPawn), bAlarm ? TEXT("최대 속도·무제한 수색 시작") : TEXT("경보 초기화: 일반 수색 규칙 복구"));
	// 추격·체포가 진행 중이면 그대로 유지한다. 나머지는 순찰/마무리 대기를 끊고 루트부터 판단한다.
	if (bAlarm && AIState != EGuardAIState::Chase)
	{
		SetAIState(EGuardAIState::Search);
		if (UBehaviorTreeComponent* BehaviorTreeComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
		{
			if (BehaviorTreeComp->IsRunning() && !BehaviorTreeComp->IsPaused())
			{
				BehaviorTreeComp->RestartTree(EBTRestartMode::ForceReevaluateRootNode);
			}
		}
	}
}

void AGuardAIController::UpdateMoveSpeedByWorldAlert(float NewGauge01)
{
	if (!HasAuthority() || bMatchEnded || !IsValid(PossessGuardPawn))
	{
		return;
	}
	UpdateWorldAlarmBehavior();
	if (IsWorldAlarmActive())
	{
		return;
	}

	// 현재 월드 경계도를 0~100 퍼센트 값으로 가져온다.
	const float WorldAlertLevel = GetWorldAlertLevel();

	// 경계도가 설정된 임계값 이상이면 속도 증가 조건이다.
	const bool bShouldSpeedUp = WorldAlertLevel >= WorldAlertSet.WorldAlertSpeedThreshold;

	// 경계도가 임계값 아래로 내려가면 다음 진입에서 다시 속도를 증가시킬 수 있도록 초기화한다.
	if (!bShouldSpeedUp)
	{
		WorldAlertSet.bWorldAlertSpeedTriggered = false;

		// 현재 속도 증가 상태가 아니라면 더 처리할 필요가 없다.
		if (!IsWorldAlertSpeedUp())
		{
			return;
		}

		// 월드 경계도에 의한 속도 증가 상태를 해제한다.
		SetWorldAlertSpeedUp(false);

		// 경계도 속도 증가를 위해 실행 중인 무소음 타이머를 정리한다.
		GuardHearingComp->ClearWorldAlertSilenceTimer();

		// 속도 변경 0928 / 작동시 삭제
		//PossessGuardPawn->GetCharacterMovement()->MaxWalkSpeed = GetNormalMoveSpeed();

		// 추격 여부까지 고려한 최종 이동 속도를 다시 적용한다.
		// 추격 중이었다면 MoveSpeedChase가 적용되고, 아니면 MoveSpeed가 적용된다.
		ApplyCurrentMoveSpeed();

		UE_LOG(LogTemp, Warning, TEXT("[GuardSpeed] 경계도 감소 - Pawn = %s | Alert = %.1f | Speed = %.1f"),
			*PossessGuardPawn->GetName(), WorldAlertLevel, GetNormalMoveSpeed());
		return;
	}

	// 아직 이번 경계도 구간에서 속도 증가가 발동되지 않았다면 속도를 증가시킨다.
	if (!WorldAlertSet.bWorldAlertSpeedTriggered)
	{
		WorldAlertSet.bWorldAlertSpeedTriggered = true;
		SetWorldAlertSpeedUp(true);

		// 속도 변경 0928 / 작동시 삭제
		//const float NewMoveSpeed = GetNormalMoveSpeed() * WorldAlertSet.WorldAlertMoveSpeedMultiplier;
		//PossessGuardPawn->GetCharacterMovement()->MaxWalkSpeed = NewMoveSpeed;

		// 추격 여부와 월드 경계도 배율을 함께 고려한 최종 이동 속도를 적용한다.
		ApplyCurrentMoveSpeed();

		UE_LOG(LogTemp, Warning, TEXT("[GuardSpeed] 속도 증가 - Pawn = %s | Alert = %.1f | SilenceTimer = %.1f sec"),
			*PossessGuardPawn->GetName(), WorldAlertLevel, GetWorldAlertSilenceDelay());

	}
	// 경계도가 올라올 때마다 무소음 타이머를 다시 시작한다.
	if (IsWorldAlertSpeedUp())
	{
		// start에 clear 같이 있음
		GuardHearingComp->StartWorldAlertSilenceTimer();
	}

	WorldAlertSet.PreviousWorldAlertLevel = WorldAlertLevel;

}

void AGuardAIController::SetChasing(bool bChasing)
{
	if (!HasAuthority())
	{
		return;
	}
	if (WorldAlertSet.bIsChasing == bChasing)
	{
		return;
	}

	WorldAlertSet.bIsChasing = bChasing;
	ApplyCurrentMoveSpeed();
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
