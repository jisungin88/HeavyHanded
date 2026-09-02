#include "Core/GameModes/HeistGameMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Alert/AlertComponent.h"
#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistEntryGate.h"
#include "Core/HeistEntryPoint.h"
#include "Core/HeistLog.h"
#include "Core/HeistSettings.h"
#include "Core/HeistStartGate.h"
#include "Core/HeistTravel.h"
#include "Core/PlayerControllers/HeistPlayerController.h"
#include "Core/RunProgressSubsystem.h"
#include "Core/VanZone.h"
#include "Loot/LootBase.h"          // hh.Result.Show 가 TSubclassOf<ALootBase> 를 이름으로 푼다
#include "Shared/NetAuthority.h"

namespace
{
	/**
	 * 접속 대기 조건을 다시 보는 주기(초).
	 * "로딩이 끝났다" 는 알림으로 오지 않아 폴링이다 — 대기 구간에서만 돌고 정수 비교뿐이다.
	 */
	constexpr float StartWaitPollSeconds = 0.25f;
}

AHeistGameMode::AHeistGameMode()
{
	GameStateClass = AHeistGameState::StaticClass();

	// 작업 레벨의 컨트롤러를 여기서 못 박는다. BP 가 아니라 C++ 인 이유는 GameStateClass 와
	// 짝이기 때문이다 — 한쪽만 BP 에 있으면 장소 BP(GM_MansionGameMode 등)를 새로 만들 때
	// 한쪽만 지정하고도 그럴듯하게 돈다
	PlayerControllerClass = AHeistPlayerController::StaticClass();

	// 관전자에게 자유 비행 폰을 주지 않는다. 엔진 기본 ASpectatorPawn 은 벽을 통과해서,
	// 체포자가 경비 · 트랩 · 금고 위치를 음성으로 알려 주면 잠입 게임이 무너진다.
	// 폰이 없으면 카메라는 팀원 시점에만 붙는다
	SpectatorClass = nullptr;

	// 페이즈는 접속 대기가 끝난 뒤에 시작한다. 엔진 기본 매치 흐름과 겹치지 않게
	// bDelayedStart 는 건드리지 않는다 — 접속 대기는 우리 쪽 타이머로만 관리한다
}

// ──────────────────────────────────────────────────────────────
// 접속 대기 → 준비 시간
// ──────────────────────────────────────────────────────────────

void AHeistGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ExpectedPlayers = ResolveExpectedPlayers(Options);

	if (ExpectedPlayers <= 0)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("올 인원을 알 수 없습니다. 마지막 접속 후 %.0f초 조용하면 시작하는 폴백으로 "
				 "동작하며, 로딩이 느린 팀원을 두고 출발할 수 있습니다."),
			UHeistSettings::Get()->PlayerJoinQuietSeconds);
	}
}

int32 AHeistGameMode::ResolveExpectedPlayers(const FString& Options) const
{
	// 1순위 — 런 진행 서브시스템. 로비에서 확정된 명단이 레벨 이동을 건너 살아 있다.
	// 서브시스템은 서버와 클라이언트가 서로 다른 객체다. 여기서 읽어도 되는 것은
	// 이 함수가 서버 전용이기 때문이고, 클라도 알아야 할 값이면 GameState 로 복제할 것
	if (const URunProgressSubsystem* Run = URunProgressSubsystem::Get(this))
	{
		const int32 RosterNum = Run->GetRosterNum();
		if (RosterNum > 0)
		{
			UE_LOG(LogHeist, Log, TEXT("이 판에 올 인원: %d명 (로비 확정 명단)"), RosterNum);
			return RosterNum;
		}
	}

	// 2순위 — URL 옵션. 명단이 채워져도 남겨 둔다.
	//   커맨드라인 · 치트로 바깥에서 주입할 수 있는 유일한 경로이고,
	//   "4명 올 예정인데 3명만 붙은" 상황을 재현하는 테스트가 이걸 쓴다.
	//       ...?game=/Script/HeavyHanded.HeistGameMode?ExpectedPlayers=2
	const int32 FromUrl = UGameplayStatics::GetIntOption(Options, TEXT("ExpectedPlayers"), 0);
	if (FromUrl > 0)
	{
		UE_LOG(LogHeist, Log, TEXT("이 판에 올 인원: %d명 (URL 지정)"), FromUrl);
		return FromUrl;
	}

	return 0;   // 모른다 — 호출부가 폴백으로 떨어진다
}

// ──────────────────────────────────────────────────────────────
// 진입점 — 전원이 같은 자리에서 시작하고 밴도 같이 간다
// ──────────────────────────────────────────────────────────────

void AHeistGameMode::ResolveEntryPoint()
{
	// 한 판의 진입점은 한 번 정해지면 끝까지 같다. 사람마다 다시 판정하면
	// 그사이 은신처 값이 바뀌었을 때(치트 · 재접속) 각자 다른 곳에서 시작한다
	if (bEntryResolved)
	{
		return;
	}

	bEntryResolved = true;

	TArray<AHeistEntryPoint*> Entries;
	AHeistEntryPoint::CollectEntryPoints(this, Entries);

	TArray<FGameplayTag> AvailableTags;
	AvailableTags.Reserve(Entries.Num());

	for (const AHeistEntryPoint* Entry : Entries)
	{
		AvailableTags.Add(Entry->GetEntryTag());
	}

	FGameplayTag SelectedTag;

	if (const URunProgressSubsystem* Run = URunProgressSubsystem::Get(this))
	{
		SelectedTag = Run->GetSelectedEntry();
	}

	// 레벨이 지정한 기본 진입점. 없으면 INDEX_NONE 이고 게이트가 첫 번째로 떨어뜨린다
	const int32 DefaultIndex = AHeistEntryPoint::FindDefaultIndex(Entries);

	const FHeistEntryResolution Resolution = HeistEntryGate::Resolve(AvailableTags, SelectedTag, DefaultIndex);

	switch (Resolution.Decision)
	{
	case EHeistEntryDecision::Selected:
		ResolvedEntry = Entries[Resolution.Index];

		UE_LOG(LogHeist, Log, TEXT("진입점 확정 — %s (%s)"),
			*ResolvedEntry->GetEntryTag().ToString(),
			*ResolvedEntry->GetDisplayName().ToString());
		break;

	case EHeistEntryDecision::Fallback:
	{
		ResolvedEntry = Entries[Resolution.Index];

		// 지정된 기본값으로 떨어진 것과 "그냥 첫 번째" 로 떨어진 것은 다르다.
		// 후자는 태그 이름에 따라 흔들리는 자리라, 레벨에 기본 진입점을 체크하라고 알려야 한다
		const bool bUsedDesignatedDefault = (Resolution.Index == DefaultIndex);

		const TCHAR* Which = bUsedDesignatedDefault
			? TEXT("기본 진입점")
			: TEXT("첫 번째 진입점(기본 지정 없음)");

		// 조용히 폴백하면 "왜 자꾸 여기서 시작하지" 가 된다. 이유를 갈라서 남긴다
		if (SelectedTag.IsValid())
		{
			UE_LOG(LogHeist, Warning,
				TEXT("진입점 %s 가 이 레벨에 없습니다. %s %s 로 시작합니다 — "
					 "레벨에서 진입점을 지웠거나 태그가 어긋났습니다."),
				*SelectedTag.ToString(), Which, *ResolvedEntry->GetEntryTag().ToString());
		}
		else
		{
			UE_LOG(LogHeist, Log,
				TEXT("고른 진입점이 없어 %s %s 로 시작합니다. (은신처를 거치지 않았거나 hh.Run.Entry 미사용)"),
				Which, *ResolvedEntry->GetEntryTag().ToString());
		}

		if (!bUsedDesignatedDefault)
		{
			UE_LOG(LogHeist, Warning,
				TEXT("이 레벨에 기본 진입점이 지정돼 있지 않습니다. 지금은 태그 이름순 첫 번째가 쓰이므로 "
					 "진입점을 추가하면 기본 시작 위치가 바뀝니다 — AHeistEntryPoint 하나에 bIsDefaultEntry 를 체크하세요."));
		}

		break;
	}

	case EHeistEntryDecision::None:
	default:
		ResolvedEntry = nullptr;

		// 테스트 맵에는 진입점이 없는 것이 정상이다. 그래서 Error 가 아니라 Log 다 —
		// 엔진 기본 스폰으로 돌아가고, 코어 루프의 나머지는 그대로 돈다
		UE_LOG(LogHeist, Log,
			TEXT("이 레벨에 AHeistEntryPoint 가 없습니다. 엔진 기본 스폰(PlayerStart 무작위)을 씁니다."));
		break;
	}

	// 은신처 값은 복제되지 않으므로 GameState 로 옮긴다. 아직 GameState 가 없을 수 있고
	// (첫 스폰이 매치 시작보다 먼저인 경로) 그때는 HandleMatchHasStarted 가 다시 넣는다
	if (AHeistGameState* GS = GetGameState<AHeistGameState>())
	{
		GS->SetEntryTag(IsValid(ResolvedEntry) ? ResolvedEntry->GetEntryTag() : FGameplayTag());
	}

	MoveVanToEntry();
}

void AHeistGameMode::MoveVanToEntry()
{
	// 재정의된 PlaceVan 은 연출일 수 있다. 두 번 부르면 밴이 두 번 달려 들어온다
	if (bVanPlaced || !IsValid(ResolvedEntry))
	{
		return;
	}

	AVanZone* Van = AVanZone::Get(this);

	if (!Van)
	{
		// 밴 없이도 진입점 스폰은 동작한다. 다만 적재·탈출이 불가능한 레벨이라는 뜻이라 경고다
		UE_LOG(LogHeist, Warning,
			TEXT("이 레벨에 AVanZone 이 없어 밴을 진입점으로 보내지 못했습니다. 적재와 탈출이 동작하지 않습니다."));
		return;
	}

	// 앵커가 없으면 **밴을 옮기지 않는다.** 진입점 자리로 보내면 그 위에서 폰이 스폰되지 못한다.
	// 밴이 레벨에 놓인 자리에 그대로 있으면 멀기는 해도 판은 돌아간다
	FTransform VanTransform;

	if (!ResolvedEntry->TryGetVanTransform(VanTransform))
	{
		UE_LOG(LogHeist, Warning,
			TEXT("밴을 옮기지 않고 레벨에 놓인 자리에 둡니다. 진입점에서 멀 수 있습니다."));
		return;
	}

	bVanPlaced = true;

	PlaceVan(Van, VanTransform);

	WarnIfVanBlocksEntry();
}

void AHeistGameMode::PlaceVan_Implementation(AVanZone* Van, const FTransform& EntryTransform)
{
	if (!IsValid(Van))
	{
		return;
	}

	// 기본 구현은 순간이동이다. 연출은 BP_HeistGameMode 에서 이 함수를 재정의해 붙인다 —
	// 어디에 서는가(EntryTransform)는 그대로 두고 어떻게 가는가만 바꾸면 된다
	Van->SetActorTransform(EntryTransform, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr,
		ETeleportType::TeleportPhysics);

	UE_LOG(LogHeist, Log, TEXT("밴을 진입점으로 옮겼습니다 — %s"),
		*EntryTransform.GetLocation().ToCompactString());
}

void AHeistGameMode::WarnIfVanBlocksEntry() const
{
	const AVanZone* Van = AVanZone::Get(this);

	if (!IsValid(ResolvedEntry) || !Van)
	{
		return;
	}

	// 밴 전체 바운즈로 본다. 어느 컴포넌트가 막았는지는 중요하지 않고,
	// "스폰 지점이 밴 안에 들어가 있다" 는 사실만 알리면 된다
	FVector Origin;
	FVector BoxExtent;
	Van->GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, BoxExtent);

	const FVector EntryLocation = ResolvedEntry->GetActorLocation();
	const FBox VanBounds(Origin - BoxExtent, Origin + BoxExtent);

	if (!VanBounds.IsInsideOrOn(EntryLocation))
	{
		return;
	}

	// 이 상황의 증상은 "이동도 회전도 안 된다" 뿐이다. 폰이 스폰되지 않았다는 사실도,
	// 그 원인이 밴이라는 것도 화면에서는 알 수 없다
	UE_LOG(LogHeist, Warning,
		TEXT("밴이 진입점 %s 를 덮고 있습니다. 폰이 스폰되지 못해 아무도 움직일 수 없게 됩니다 — "
			 "진입점의 VanAnchor 화살표를 스폰 지점에서 떨어뜨려 놓으세요."),
		*ResolvedEntry->GetEntryTag().ToString());
}

AActor* AHeistGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 첫 스폰이 매치 시작보다 먼저 올 수 있다 (리슨 서버 호스트).
	// 그래서 여기서도 한 번 확인한다 — 안에서 이미 정해졌으면 즉시 돌아온다
	ResolveEntryPoint();

	if (IsValid(ResolvedEntry))
	{
		// 전원이 같은 진입점이다. 엔진 기본 구현처럼 "빈 곳" 을 찾지 않는다 —
		// 밴에서 같이 내리는 것이 전제라 겹치는 것이 정상이고, 캡슐은 서로 밀어낸다
		return ResolvedEntry;
	}

	// 진입점이 없는 레벨(테스트 맵)에서는 엔진 기본 동작으로 돌아간다
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AHeistGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	// 아무도 아직 스폰되지 않았을 수 있다. 진입점과 밴은 준비 시간보다 먼저 자리를 잡아야 한다
	ResolveEntryPoint();

	AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		UE_LOG(LogHeist, Error,
			TEXT("GameState 가 AHeistGameState 가 아닙니다. 코어 루프가 돌지 않습니다 — "
				 "World Settings 의 GameState 클래스를 확인하세요."));
		return;
	}

	GS->SetTargetValue(TargetValue);

	// 진입점 판정이 첫 스폰에서 먼저 끝났다면 그때는 GameState 가 없었을 수 있다.
	// 여기서 한 번 더 넣는다 — 이미 같은 값이면 복제가 dirty 되지 않는다
	GS->SetEntryTag(IsValid(ResolvedEntry) ? ResolvedEntry->GetEntryTag() : FGameplayTag());

	// 경보와 승차를 여기서 구독한다. AlertComponent 는 GameState 가 만들어질 때 붙으므로
	// (UNoiseSubsystem 의 GameStateSetEvent) 이 시점에는 이미 있다.
	if (UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		Alert->OnAlertLevelChanged.AddDynamic(this, &AHeistGameMode::HandleAlertLevelChanged);
	}
	else
	{
		UE_LOG(LogHeist, Warning,
			TEXT("GameState 에 AlertComponent 가 없어 경보로 도주에 들어가지 못합니다. "
				 "제한 시간 만료 경로만 동작합니다."));
	}

	GS->OnBoardedChanged.AddDynamic(this, &AHeistGameMode::HandleBoardedChanged);
	GS->OnResultConfirmChanged.AddDynamic(this, &AHeistGameMode::HandleResultConfirmChanged);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	StartDeadline = Now + UHeistSettings::Get()->PlayerJoinTimeoutSeconds;
	LastLoginTime = Now;
	bStartWindowOpen = true;

	UE_LOG(LogHeist, Log, TEXT("접속 대기 시작 — 목표 $%d / 제한 %.0f초, 상한 %.0f초"),
		TargetValue, HeistSeconds, UHeistSettings::Get()->PlayerJoinTimeoutSeconds);

	GetWorldTimerManager().SetTimer(StartWaitHandle, this, &AHeistGameMode::TickStartWait,
		StartWaitPollSeconds, true);

	TickStartWait();   // 혼자 시작하는 경우 첫 주기를 기다릴 이유가 없다
}

FString AHeistGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ErrorMessage.IsEmpty())
	{
		return ErrorMessage;
	}

	const URunProgressSubsystem* Run = URunProgressSubsystem::Get(this);
	if (!Run || !Run->IsArrested(UniqueId))
	{
		return ErrorMessage;
	}

	APlayerState* PS = NewPlayerController ? NewPlayerController->PlayerState : nullptr;
	if (!PS)
	{
		// Super 가 PlayerState 를 만들지 못했다면 접속 자체가 성립하지 않은 것이다.
		// 조용히 넘기면 체포자가 그대로 플레이하게 되므로 남긴다
		UE_LOG(LogHeist, Warning,
			TEXT("체포자의 PlayerState 가 없어 관전으로 넘기지 못했습니다. 그대로 플레이합니다."));
		return ErrorMessage;
	}

	// 이 한 줄이 관전의 전부다. 폰 스폰과 인원 집계는 엔진이 MustSpectate 를 보고 알아서 한다.
	// 카메라를 어디에 둘지는 로컬 문제라 AHeistPlayerController 가 정한다
	PS->SetIsOnlyASpectator(true);

	UE_LOG(LogHeist, Log, TEXT("체포자 %s — 이 판은 관전한다."), *PS->GetPlayerName());

	return ErrorMessage;
}

void AHeistGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 접속 대기와 무관하게, 들어온 사람은 전부 다운 감시 대상이다.
	// 아래 이른 반환보다 위에 있어야 하는 이유가 이것이다 — 판이 시작된 뒤 들어온
	// 사람(재접속)도 감시해야 한다
	if (NewPlayer)
	{
		WatchDownedState(NewPlayer->PlayerState);
	}

	if (!bStartWindowOpen || bStarted)
	{
		return;
	}

	// 폴백 판정의 기준점만 갱신한다. 시작 여부는 TickStartWait 이 혼자 결정한다
	if (const UWorld* World = GetWorld())
	{
		LastLoginTime = World->GetTimeSeconds();
	}

	UE_LOG(LogHeist, Verbose, TEXT("접속 — 합계 %d명 (로딩 중 %d명) / 예정 %d명"),
		GetNumPlayers(), NumTravellingPlayers, ExpectedPlayers);
}

void AHeistGameMode::Logout(AController* Exiting)
{
	// 나간 사람은 승차 명단에서 빠져야 한다. 안 그러면 "안 탄 사람" 으로 남아
	// 남은 팀원이 전원 승차를 영영 성립시키지 못하고 도주 시간을 끝까지 기다린다.
	if (AHeistGameState* GS = GetGameState<AHeistGameState>())
	{
		if (const APlayerController* PC = Cast<APlayerController>(Exiting))
		{
			GS->SetBoarded(PC->PlayerState, false);
		}
	}

	Super::Logout(Exiting);

	// 지금 판정하면 PlayerArray 에 나가는 사람이 아직 남아 있어 생존자 수가 한 명 많다.
	// 그 값으로 판정하면 끝날 판이 안 끝난다 — 다른 두 계기와 같은 이유로 다음 틱에 본다
	RequestEscapeCheck();
}

FHeistStartConditions AHeistGameMode::MakeStartConditions()
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	FHeistStartConditions Conditions;
	// 관전자를 더한다. GetNumPlayers() 는 MustSpectate 인 접속을 세지 않아서, 체포자가
	// 한 명이라도 있으면 인원이 영영 예정치에 못 미쳐 **매 판 상한을 다 기다린다.**
	// 관전자의 로딩 완료 여부는 엔진이 알려주지 않으므로 구분하지 않는다
	Conditions.NumPlayers = GetNumPlayers() + GetNumSpectators();
	Conditions.NumTravellingPlayers = NumTravellingPlayers;
	Conditions.ExpectedPlayers = ExpectedPlayers;
	Conditions.SecondsSinceLastLogin = Now - LastLoginTime;
	Conditions.SecondsUntilDeadline = StartDeadline - Now;
	Conditions.QuietSeconds = UHeistSettings::Get()->PlayerJoinQuietSeconds;

	return Conditions;
}

void AHeistGameMode::TickStartWait()
{
	if (bStarted)
	{
		return;
	}

	const FHeistStartConditions Conditions = MakeStartConditions();

	switch (HeistStartGate::Evaluate(Conditions))
	{
	case EHeistStartDecision::Wait:
		// 아직 기다린다. 클라이언트가 "무엇을 기다리는지" 를 알아야 로딩 표시를 띄우고
		// 조작을 막을 수 있다 — 판정은 서버가 하지만 표시와 입력은 각자 자기 화면의 몫이다
		PublishStartWait(Conditions, /*bWaiting=*/true);
		return;

	case EHeistStartDecision::TimedOut:
		UE_LOG(LogHeist, Warning,
			TEXT("접속 대기 상한 %.0f초 초과 — %d명(로딩 중 %d명)으로 시작합니다. 예정 인원 %d명."),
			UHeistSettings::Get()->PlayerJoinTimeoutSeconds,
			Conditions.NumPlayers, Conditions.NumTravellingPlayers, Conditions.ExpectedPlayers);
		break;

	case EHeistStartDecision::Ready:
		break;
	}

	StartPrep();
}

void AHeistGameMode::StartPrep()
{
	if (bStarted)
	{
		return;
	}

	bStarted = true;
	GetWorldTimerManager().ClearTimer(StartWaitHandle);

	UE_LOG(LogHeist, Log, TEXT("접속 대기 종료 — %d명으로 시작합니다."), GetNumPlayers());

	// 페이즈보다 **먼저** 대기 해제를 알린다. 순서가 뒤집히면 Prep 이 도착한 프레임에도
	// 입력이 아직 막혀 있어, 45초짜리 준비 시간의 앞부분을 한 틱 잃는다
	PublishStartWait(MakeStartConditions(), /*bWaiting=*/false);

	EnterPhase(HHTags::Phase_Prep, EHeistPhaseReason::Scheduled);
}

void AHeistGameMode::PublishStartWait(const FHeistStartConditions& Conditions, bool bWaiting)
{
	AHeistGameState* GS = GetGameState<AHeistGameState>();

	// 대기 초반에는 GameState 가 아직 없을 수 있다. 다음 폴링에서 다시 온다
	if (!GS)
	{
		return;
	}

	FHeistStartWaitState State;
	State.bWaiting = bWaiting;
	State.NumConnected = Conditions.NumPlayers;
	State.NumExpected = Conditions.ExpectedPlayers;
	State.NumTravelling = Conditions.NumTravellingPlayers;

	GS->SetStartWaitState(State);
}

// ──────────────────────────────────────────────────────────────
// 페이즈 전이
// ──────────────────────────────────────────────────────────────

float AHeistGameMode::GetPhaseDuration(const FGameplayTag& Phase) const
{
	const UHeistSettings* Settings = UHeistSettings::Get();

	if (Phase == HHTags::Phase_Prep)
	{
		return Settings->PrepSeconds;
	}

	if (Phase == HHTags::Phase_Heist)
	{
		return HeistSeconds;   // 장소마다 달라서 Settings 가 아니라 이 클래스의 값이다
	}

	if (Phase == HHTags::Phase_Escape)
	{
		return Settings->EscapeSeconds;
	}

	if (Phase == HHTags::Phase_Result)
	{
		// 결과 화면도 카운트다운을 갖는다. 전원이 확인하면 그전에 넘어가고,
		// 이 시간은 아무도 누르지 않았을 때의 안전망이다
		return Settings->ResultSeconds;
	}

	return 0.f;
}

void AHeistGameMode::EnterPhase(const FGameplayTag& Phase, EHeistPhaseReason Reason)
{
	AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		return;
	}

	const float Duration = GetPhaseDuration(Phase);
	GS->SetPhase(Phase, Duration, Reason);

	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(PhaseTimerHandle);

	if (Duration > 0.f)
	{
		Timers.SetTimer(PhaseTimerHandle, this, &AHeistGameMode::HandlePhaseElapsed, Duration, false);
	}

	OnPhaseEntered(Phase, Reason);
}

void AHeistGameMode::OnPhaseEntered(const FGameplayTag& Phase, EHeistPhaseReason Reason)
{
	// 본 작업에 들어가는 순간 경계도를 0 으로 되돌린다.
	// 준비 시간의 드론 정찰과 발소리까지 세면 시작부터 손해를 보고 들어간다
	if (Phase == HHTags::Phase_Heist)
	{
		if (UAlertComponent* Alert = UAlertComponent::Get(this))
		{
			Alert->ResetAlert();
		}
		else
		{
			UE_LOG(LogHeist, Warning,
				TEXT("GameState 에 AlertComponent 가 없어 준비 시간의 경계도가 남습니다."));
		}

		// 여기서 탈출 판정을 부르지 않는다. 전원이 아직 밴 근처라 스폰 위치가 승차 볼륨과
		// 겹치면 그 자리에서 $0 으로 판이 끝난다. 판정은 '변화' 로만 성립하게 둔다
	}

	if (Phase == HHTags::Phase_Result)
	{
		// 순서가 규칙이다. 체포를 먼저 확정해야 "빠져나온 사람이 있는가" 를 셀 수 있고,
		// 등급이 나와야 지급 여부와 장소 통과 여부를 판정할 수 있다.
		ResolveArrests();

		if (AHeistGameState* GS = GetGameState<AHeistGameState>())
		{
			GS->FinalizeOutcome();
		}

		PayoutTeamGold();
		CarryOverArrests();
		ReleaseServedSpectators();
		RecordSiteProgress();
	}
}

// ──────────────────────────────────────────────────────────────
// 도주 — 경보와 탈출
// ──────────────────────────────────────────────────────────────

void AHeistGameMode::HandleAlertLevelChanged(EAlertLevel NewLevel, EAlertLevel /*OldLevel*/)
{
	if (NewLevel != EAlertLevel::Alarm)
	{
		return;
	}

	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS || !GS->IsPhase(HHTags::Phase_Heist))
	{
		// 준비 시간이면 무시한다 — Heist 진입에서 경계도가 리셋되므로 이 경보는 사라진다.
		// 이미 도주 · 결과면 늦었다. 어느 쪽이든 지금 페이즈를 건드릴 이유가 없다
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("경보 발생 — 본 작업을 끝내고 도주로 넘어갑니다."));

	EnterPhase(HHTags::Phase_Escape, EHeistPhaseReason::Alarm);
}

void AHeistGameMode::HandleBoardedChanged(int32 /*NumBoarded*/, int32 /*NumSurvivors*/)
{
	RequestEscapeCheck();
}

void AHeistGameMode::WatchDownedState(APlayerState* Player)
{
	IAbilitySystemInterface* AsAbilitySystem = Cast<IAbilitySystemInterface>(Player);
	if (!AsAbilitySystem)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = AsAbilitySystem->GetAbilitySystemComponent())
	{
		// 델리게이트는 ASC 가 들고 있다. PlayerState 가 사라지면 같이 사라지므로
		// 따로 해제할 것이 없다
		ASC->RegisterGameplayTagEvent(HHTags::State_Downed, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &AHeistGameMode::HandleDownedTagChanged);
	}
}

void AHeistGameMode::HandleDownedTagChanged(const FGameplayTag /*Tag*/, int32 /*NewCount*/)
{
	// 다운으로 생존자가 줄면 이미 타 있던 사람들만으로 전원 승차가 성립할 수 있다.
	// 여기는 특히 즉시 판정하면 안 된다 — ASC 태그 콜백 안이라, 그 자리에서 페이즈를
	// 넘기면 태그를 바꾸던 GameplayEffect 처리 도중에 판이 끝난다
	RequestEscapeCheck();
}

void AHeistGameMode::RequestEscapeCheck()
{
	if (bEscapeCheckQueued)
	{
		return;
	}

	bEscapeCheckQueued = true;
	GetWorldTimerManager().SetTimerForNextTick(this, &AHeistGameMode::TryFinishByEscape);
}

void AHeistGameMode::TryFinishByEscape()
{
	bEscapeCheckQueued = false;

	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		return;
	}

	// 본 작업과 도주에서만 센다. 준비 시간은 밴 근처가 시작 지점이라 전원이 볼륨 안에 있고,
	// 결과는 이미 끝난 판이다
	if (!GS->IsPhase(HHTags::Phase_Heist) && !GS->IsPhase(HHTags::Phase_Escape))
	{
		return;
	}

	if (!HeistEscapeGate::HasEveryoneEscaped(GS->MakeEscapeConditions()))
	{
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("생존자 전원 승차 — 판을 끝냅니다. 적재 $%d / $%d"),
		GS->GetLoadedValue(), GS->GetTargetValue());

	EnterPhase(HHTags::Phase_Result, EHeistPhaseReason::AllEscaped);
}

void AHeistGameMode::ResolveArrests()
{
	AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		return;
	}

	// 배열을 복사해 도는 이유는 MarkArrested 가 GameState 의 다른 배열을 건드리기 때문이다.
	// PlayerArray 자체는 안 바뀌지만, 여기서 순회 중에 누가 나가면 아래에서 터진다
	TArray<TObjectPtr<APlayerState>> Players = GS->PlayerArray;
	int32 EscapedNum = 0;

	for (APlayerState* Player : Players)
	{
		// 세는 모집단은 GameState 와 같아야 한다. 여기서 조건을 다시 적으면 한쪽만 고쳐졌을 때
		// 체포되지 않은 사람이 생기고, 전원 탈출을 체포 명단으로 보는 FinalizeOutcome 이
		// 그를 탈출로 세어 등급이 조용히 한 칸 올라간다
		if (!AHeistGameState::IsCountedPlayer(Player))
		{
			continue;
		}

		// 밴에 타 있어도 다운 상태면 체포다 (기획 확정). 쓰러진 채로 실려 나가는 것은
		// 탈출이 아니라 붙잡힌 것이다
		const bool bEscaped = GS->IsBoarded(Player) && !AHeistGameState::IsPlayerDowned(Player);

		if (!bEscaped)
		{
			GS->MarkArrested(Player);
		}
		else
		{
			++EscapedNum;
		}
	}

	UE_LOG(LogHeist, Log, TEXT("결과 확정 — 탈출 %d명 / 체포 %d명"),
		EscapedNum, GS->GetArrestedPlayers().Num());
}

void AHeistGameMode::PayoutTeamGold()
{
	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		return;
	}

	const EHeistOutcome MinOutcome = UHeistSettings::Get()->MinOutcomeForPayout;

	// 등급이 Failure < Partial < Success 순서라 비교 하나로 끝난다 (EHeistOutcome 주석 참고)
	if (GS->GetOutcome() < MinOutcome)
	{
		UE_LOG(LogHeist, Log, TEXT("정산 없음 — 등급 %s (지급 기준 %s 이상). 적재 $%d 는 소멸한다."),
			HeistOutcome::ToString(GS->GetOutcome()), HeistOutcome::ToString(MinOutcome),
			GS->GetLoadedValue());
		return;
	}

	// 은신처 정산 · 장비 구매의 입력이다. 이 서브시스템은 복제되지 않고 서버 것만 유효한데,
	// 여기가 서버 전용(GameMode)이라 안전하다 — 클라이언트가 알아야 하는 잔액이 생기면
	// 서브시스템이 아니라 GameState 로 복제할 것.
	URunProgressSubsystem* Run = URunProgressSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("URunProgressSubsystem 이 없어 적재 $%d 를 팀 골드로 넘기지 못했습니다."),
			GS->GetLoadedValue());
		return;
	}

	Run->AddTeamGold(GS->GetLoadedValue());

	UE_LOG(LogHeist, Log, TEXT("정산 — 적재 $%d 를 팀 골드로 적립. 잔액 $%d"),
		GS->GetLoadedValue(), Run->GetTeamGold());
}

void AHeistGameMode::RecordSiteProgress()
{
	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		return;
	}

	// 통과는 '작업 성공' 뿐이다 (기획서 2장 — 실패한 장소는 처음부터 재시작).
	// 목표를 못 채운 판(Partial)은 실어 온 돈은 받아 가지만 장소는 다시 해야 한다.
	if (GS->GetOutcome() < EHeistOutcome::Success)
	{
		UE_LOG(LogHeist, Log, TEXT("장소 미통과 — 등급 %s. 이 장소는 재도전 대상이다."),
			HeistOutcome::ToString(GS->GetOutcome()));
		return;
	}

	URunProgressSubsystem* Run = URunProgressSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("URunProgressSubsystem 이 없어 장소 통과를 기록하지 못했습니다."));
		return;
	}

	// 무효 태그 경고는 서브시스템이 남긴다 — 목록을 오염시키지 않는 것이 그쪽 책임이라
	// 판정을 여기서 한 번 더 적지 않는다
	Run->RecordSiteCleared(SiteTag);
}

void AHeistGameMode::CarryOverArrests()
{
	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS || GS->GetArrestedPlayers().IsEmpty())
	{
		return;
	}

	URunProgressSubsystem* Run = URunProgressSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("URunProgressSubsystem 이 없어 체포 %d명을 다음 판으로 넘기지 못했습니다."),
			GS->GetArrestedPlayers().Num());
		return;
	}

	// PlayerState 포인터는 레벨이 바뀌면 전부 죽는다. 신원(FUniqueNetIdRepl)만 넘긴다 —
	// 서브시스템이 역할과 명단을 같은 키로 들고 있는 이유와 같다
	TArray<FUniqueNetIdRepl> ArrestedIds;
	ArrestedIds.Reserve(GS->GetArrestedPlayers().Num());

	for (const APlayerState* Player : GS->GetArrestedPlayers())
	{
		if (IsValid(Player))
		{
			ArrestedIds.Add(Player->GetUniqueId());
		}
	}

	Run->RecordArrested(ArrestedIds);
}

void AHeistGameMode::ReleaseServedSpectators()
{
	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		return;
	}

	URunProgressSubsystem* Run = URunProgressSubsystem::Get(this);
	if (!Run || Run->GetArrestedNum() == 0)
	{
		return;
	}

	// 이번 판을 관전으로 보낸 사람들. IsCountedPlayer 가 관전자를 걸러내므로 이들은
	// 적재 · 승차 · 체포 어디에도 나타나지 않았다 — PlayerArray 를 직접 훑는 수밖에 없다
	TArray<FUniqueNetIdRepl> ServedIds;

	for (const APlayerState* Player : GS->PlayerArray)
	{
		if (IsValid(Player) && Player->IsOnlyASpectator())
		{
			ServedIds.Add(Player->GetUniqueId());
		}
	}

	// 접속을 끊고 나간 관전자는 여기 잡히지 않아 체포가 그대로 남는다. 그게 맞다 —
	// 형기는 판을 끝까지 지켜본 대가이고, 나갔다 들어온 것으로 면제되면 안 된다
	Run->ReleaseArrested(ServedIds);
}

void AHeistGameMode::HandlePhaseElapsed()
{
	// 결과 화면의 만료는 다음 페이즈로 가는 것이 아니라 매치의 끝이다.
	// AdvancePhase 에 맡기면 GetNext(Result) 가 무효라 조용히 아무 일도 일어나지 않고,
	// 아무도 결과 화면에서 나가지 못한다
	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (GS && GS->IsPhase(HHTags::Phase_Result))
	{
		UE_LOG(LogHeist, Log, TEXT("결과 체류 시간 종료 — 매치를 끝냅니다."));
		FinishMatch();
		return;
	}

	// 타이머가 만료됐다는 것은 곧 "시간이 다 됐다" 는 뜻이다.
	// 경보로 인한 조기 진입은 이 경로가 아니라 AdvancePhase 로 들어온다
	AdvancePhase(EHeistPhaseReason::Scheduled);
}

void AHeistGameMode::HandleResultConfirmChanged(int32 NumConfirmed, int32 TotalNum)
{
	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS || !GS->IsPhase(HHTags::Phase_Result))
	{
		return;
	}

	if (!GS->AreAllResultsConfirmed())
	{
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("전원 확인 (%d/%d) — 체류 시간을 기다리지 않고 끝냅니다."),
		NumConfirmed, TotalNum);

	FinishMatch();
}

void AHeistGameMode::FinishMatch_Implementation()
{
	if (bFinished)
	{
		return;
	}

	bFinished = true;
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	// 은신처로 돌려보낸다. 안 하면 사이클이 닫히지 않아 상점 · 구출 · 목표 선택에 도달할 수 없다.
	// BlueprintNativeEvent 라 BP 가 재정의하면 여기는 실행되지 않는다
	const FSoftObjectPath HideoutPath = UHeistSettings::Get()->GetHideoutLevel();

	// 지정돼 있지 않으면 떠나지 않는다. TryDepartToSite 와 같은 판단이다 —
	// "모르겠으면 기본 맵" 으로 폴백하면 전원이 엉뚱한 곳으로 끌려가고 되돌릴 방법이 없다
	if (HideoutPath.IsNull())
	{
		UE_LOG(LogHeist, Warning,
			TEXT("매치 종료 — 은신처 레벨이 지정되지 않아 결과 화면에 머뭅니다. "
				 "Project Settings → Game → Heist → Hideout Level 을 지정하세요."));
		return;
	}

	// 인원 수를 0 으로 넘겨 ?ExpectedPlayers 를 붙이지 않는다. 접속 대기는 작업 레벨에만
	// 있고(AHeistGameMode), 은신처는 기다릴 것이 없어 그 옵션을 읽는 쪽이 없다.
	// URL 조립을 TryDepartToSite 와 같은 함수에 맡기는 것은 패키지 이름 추출이 한 곳이어야
	// 하기 때문이다 — 소프트 경로를 그대로 넘기면 맵을 못 찾는데 그 실패는 조용하다
	const FString TravelURL = HeistTravel::BuildTravelURL(HideoutPath, 0);
	if (TravelURL.IsEmpty())
	{
		UE_LOG(LogHeist, Warning,
			TEXT("매치 종료 — 은신처 경로 '%s' 에서 패키지 이름을 얻지 못해 이동하지 못했습니다."),
			*HideoutPath.ToString());
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("매치 종료 — 은신처로 돌아갑니다 → %s"), *TravelURL);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 비-심리스 travel 이다. 이 줄 뒤로 같은 월드가 이어지지 않으므로 결과 확정
	// (체포 · 등급 · 정산 · 장소 통과)은 전부 Phase.Result 진입에서 이미 끝나 있어야 한다.
	// 레벨을 건너 살아남는 것은 URunProgressSubsystem 뿐이다
	World->ServerTravel(TravelURL);
}

void AHeistGameMode::AdvancePhase(EHeistPhaseReason Reason)
{
	if (!bStarted)
	{
		StartPrep();
		return;
	}

	const AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		return;
	}

	const FGameplayTag Next = HeistPhase::GetNext(GS->GetCurrentPhase());
	if (!Next.IsValid())
	{
		return;   // Result 가 끝이다
	}

	EnterPhase(Next, Reason);
}

// ──────────────────────────────────────────────────────────────
// 치트
// ──────────────────────────────────────────────────────────────

// 준비 45초 + 본 작업 7분을 매번 앉아서 기다리면 탈출 판정 한 줄 고치는 데 8분이 든다.
// 서버에서만 듣는다 — 클라이언트 창에서 쳐도 아무 일도 일어나지 않는다
static void PhaseNextCommand(UWorld* World)
{
	if (!HasServerAuthority(World))
	{
		UE_LOG(LogHeist, Warning, TEXT("hh.Phase.Next 는 서버(호스트) 창에서만 동작합니다."));
		return;
	}

	AHeistGameMode* GameMode = World->GetAuthGameMode<AHeistGameMode>();
	if (!GameMode)
	{
		UE_LOG(LogHeist, Warning, TEXT("작업 레벨이 아닙니다 — AHeistGameMode 가 없습니다."));
		return;
	}

	GameMode->AdvancePhase(EHeistPhaseReason::Cheat);
}

static FAutoConsoleCommandWithWorld GPhaseNextCommand(
	  TEXT("hh.Phase.Next"),
	  TEXT("지금 페이즈를 즉시 끝내고 다음으로 넘긴다. 접속 대기 중이면 준비 시간을 시작한다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&PhaseNextCommand),
	  ECVF_Cheat);

// "지금 무슨 페이즈이고 몇 초 남았나" 는 서버 · 클라 양쪽에서 물어볼 일이 생긴다.
// 클라이언트 창에서도 답이 나와야 복제가 제대로 됐는지 확인할 수 있다
static void PhaseShowCommand(UWorld* World)
{
	const AHeistGameState* GS = AHeistGameState::Get(World);
	if (!GS)
	{
		UE_LOG(LogHeist, Warning, TEXT("작업 레벨이 아닙니다 — AHeistGameState 가 없습니다."));
		return;
	}

	const FGameplayTag Phase = GS->GetCurrentPhase();

	float Remaining = 0.f;
	const bool bHasCountdown = GS->TryGetPhaseRemainingSeconds(Remaining);

	UE_LOG(LogHeist, Log, TEXT("페이즈 %s (사유: %s) / 남은 시간 %s / 적재 $%d of $%d / 승차 %d of %d"),
		Phase.IsValid() ? *Phase.ToString() : TEXT("(접속 대기)"),
		HeistPhase::ToString(GS->GetPhaseReason()),
		bHasCountdown ? *FString::Printf(TEXT("%.1f초"), Remaining) : TEXT("없음"),
		GS->GetLoadedValue(), GS->GetTargetValue(),
		GS->GetBoardedNum(), GS->GetSurvivorNum());
}

static FAutoConsoleCommandWithWorld GPhaseShowCommand(
	  TEXT("hh.Phase.Show"),
	  TEXT("현재 페이즈 · 남은 시간 · 적재 금액을 찍는다. 클라이언트 창에서도 동작한다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&PhaseShowCommand),
	  ECVF_Cheat);

// 결과 화면(오유석)이 붙기 전까지 Result 데이터를 눈으로 확인할 유일한 수단이다.
// 클라이언트 창에서도 돌아야 한다 — 복제가 실제로 갔는지가 확인의 핵심이라,
// 서버에서만 찍으면 "서버에는 있는데 화면에 안 나온다" 를 구별할 수 없다
static void ResultShowCommand(UWorld* World)
{
	const AHeistGameState* GS = AHeistGameState::Get(World);
	if (!GS)
	{
		UE_LOG(LogHeist, Warning, TEXT("작업 레벨이 아닙니다 — AHeistGameState 가 없습니다."));
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("── 결과 ── %s / 사유 %s / 적재 $%d of $%d / 소요 %.1f초"),
		HeistOutcome::ToString(GS->GetOutcome()),
		HeistPhase::ToString(GS->GetPhaseReason()),
		GS->GetLoadedValue(), GS->GetTargetValue(), GS->GetElapsedSeconds());

	// 팀 골드는 서버에만 있다 (URunProgressSubsystem 은 복제되지 않는다).
	// 클라이언트 창에서는 이 줄이 나오지 않는 것이 정상이다
	if (const URunProgressSubsystem* Run = URunProgressSubsystem::Get(World))
	{
		UE_LOG(LogHeist, Log, TEXT("팀 골드 $%d (지급 기준 %s 이상)"),
			Run->GetTeamGold(),
			HeistOutcome::ToString(UHeistSettings::Get()->MinOutcomeForPayout));
	}

	const TArray<FHeistLoadEntry>& Entries = GS->GetLoadedEntries();
	UE_LOG(LogHeist, Log, TEXT("적재 목록 %d건"), Entries.Num());

	for (const FHeistLoadEntry& Entry : Entries)
	{
		UE_LOG(LogHeist, Log, TEXT("  %s $%d%s — %s"),
			*GetNameSafe(Entry.LootClass),
			Entry.Value,
			Entry.IsValueLost() ? *FString::Printf(TEXT(" (원래 $%d)"), Entry.BaseValue) : TEXT(""),
			Entry.Loader ? *Entry.Loader->GetPlayerName() : TEXT("(주인 없음)"));
	}

	for (const APlayerState* Player : GS->PlayerArray)
	{
		if (!IsValid(Player))
		{
			continue;
		}

		UE_LOG(LogHeist, Log, TEXT("  %s — %s / 기여 $%d"),
			*Player->GetPlayerName(),
			GS->IsArrested(Player) ? TEXT("체포") : (GS->IsBoarded(Player) ? TEXT("탈출") : TEXT("미승차")),
			GS->GetContributionOf(Player));
	}

	float NoiseContribution = 0.f;
	const APlayerState* Noisiest = GS->GetNoisiestPlayer(NoiseContribution);

	UE_LOG(LogHeist, Log, TEXT("최다 소음 유발자: %s (%.1f)"),
		Noisiest ? *Noisiest->GetPlayerName() : TEXT("(없음)"), NoiseContribution);
}

// HUD 확인 버튼이 붙기 전까지 전원 확인 경로를 확인할 유일한 수단이다.
// 서버에서만 듣는다 — 클라이언트 → 서버 경로(PlayerController 의 Server RPC)는
// 세션 · UI 파트가 붙일 몫이고, 그게 없는 지금은 호스트 창에서만 넣을 수 있다
static void ResultConfirmCommand(UWorld* World)
{
	if (!HasServerAuthority(World))
	{
		UE_LOG(LogHeist, Warning,
			TEXT("hh.Result.Confirm 은 서버(호스트) 창에서만 동작합니다. "
				 "클라이언트 확인은 HUD 의 Server RPC 가 붙어야 합니다."));
		return;
	}

	AHeistGameState* GS = AHeistGameState::Get(World);
	if (!GS)
	{
		UE_LOG(LogHeist, Warning, TEXT("작업 레벨이 아닙니다 — AHeistGameState 가 없습니다."));
		return;
	}

	// 접속 중인 전원을 확인 처리한다. 한 명씩 넣을 수단이 없어서,
	// "전원 확인이면 즉시 끝난다" 를 보는 것이 이 명령의 목적이다
	for (APlayerState* Player : GS->PlayerArray)
	{
		GS->SetResultConfirmed(Player, true);
	}
}

static FAutoConsoleCommandWithWorld GResultConfirmCommand(
	  TEXT("hh.Result.Confirm"),
	  TEXT("접속 중인 전원을 결과 확인 처리한다. 체류 시간을 기다리지 않고 매치가 끝난다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&ResultConfirmCommand),
	  ECVF_Cheat);

static FAutoConsoleCommandWithWorld GResultShowCommand(
	  TEXT("hh.Result.Show"),
	  TEXT("결과 데이터를 전부 찍는다 — 적재 목록 · 기여도 · 탈출/체포 · 소요 시간 · 최다 소음 유발자"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&ResultShowCommand),
	  ECVF_Cheat);



static APawn* FindPawnForPlayerState(UWorld* World, const APlayerState* Target)
{
	if (APawn* Pawn = Target->GetPawn())
	{
		return Pawn;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (PC->PlayerState == Target)
			{
				return PC->GetPawn();
			}
		}
	}
	return nullptr;
}

static void SpectateForceCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!HasServerAuthority(World))
	{
		UE_LOG(LogHeist, Warning, TEXT("hh.Spectate.Force 는 서버(호스트) 창에서만 동작합니다."));
		return;
	}

	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		UE_LOG(LogHeist, Warning, TEXT("GameState 가 없습니다."));
		return;
	}

	const int32 Index = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
	if (!GS->PlayerArray.IsValidIndex(Index))
	{
		UE_LOG(LogHeist, Warning, TEXT("플레이어 인덱스 %d 가 없습니다 (접속 %d명)."),
					  Index, GS->PlayerArray.Num());
		return;
	}

	APlayerState* Target = GS->PlayerArray[Index];
	if (!IsValid(Target))
	{
		return;
	}

	if (Target->IsOnlyASpectator())
	{
		UE_LOG(LogHeist, Log, TEXT("%s 는 이미 관전자입니다."), *Target->GetPlayerName());
		return;
	}

	// 정상 경로(InitNewPlayer)와 같은 신호
	Target->SetIsOnlyASpectator(true);

	// 폰 삭제
	APawn* Pawn = FindPawnForPlayerState(World, Target);
	UE_LOG(LogHeist, Log, TEXT("[Force] %s / PawnPrivate=%s / 찾은 폰=%s / 컨트롤러=%s"),
			  *Target->GetPlayerName(),
			  *GetNameSafe(Target->GetPawn()),
			  *GetNameSafe(Pawn),
			  Pawn ? *GetNameSafe(Pawn->GetController()) : TEXT("(없음)"));

	if (Pawn)
	{
		AController* PawnController = Pawn->GetController();
		if (PawnController)
		{
			PawnController->UnPossess();
		}

		const bool bDestroyed = Pawn->Destroy();
		UE_LOG(LogHeist, Log, TEXT("[Force] Destroy %s"),
					  bDestroyed ? TEXT("성공") : TEXT("실패"));

		if (APlayerController* TargetPC = Cast<APlayerController>(PawnController))
		{
			TargetPC->ChangeState(NAME_Spectating);
			TargetPC->ClientGotoState(NAME_Spectating);
		}
	}
	else
	{
		UE_LOG(LogHeist, Warning, TEXT("[Force] %s 의 폰을 찾지 못했습니다."), *Target->GetPlayerName());
	}

	UE_LOG(LogHeist, Log,
			  TEXT("%s 를 관전자로 만들었습니다. 되돌릴 수 없습니다 — 판을 다시 시작할 것."),
			  *Target->GetPlayerName());
}

static FAutoConsoleCommandWithWorldAndArgs GSpectateForceCommand(
		TEXT("hh.Spectate.Force"),
		TEXT("hh.Spectate.Force [인덱스] — 해당 플레이어를 지금 이 판에서 관전자로 만든다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpectateForceCommand),
		ECVF_Cheat);
