#include "Core/GameModes/HeistGameMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Alert/AlertComponent.h"
#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistLog.h"
#include "Core/HeistSettings.h"
#include "Core/HeistStartGate.h"
#include "Core/RunProgressSubsystem.h"
#include "Shared/NetAuthority.h"

namespace
{
	/**
	 * 접속 대기 조건을 다시 보는 주기(초).
	 *
	 * "로딩이 끝났다" 와 "인원이 다 찼다" 는 알림으로 오지 않는다. NumTravellingPlayers 는
	 * 엔진이 조용히 깎는 카운터고, PostLogin 은 접속 시점이지 로딩 완료 시점이 아니다.
	 * 그래서 폴링이다. 대기 구간에서만 도는 데다 하는 일이 정수 비교 몇 개뿐이라 비용이 없다.
	 *
	 * 밸런싱 값이 아니라 구현 상수라 UHeistSettings 에 두지 않는다.
	 */
	constexpr float StartWaitPollSeconds = 0.25f;
}

AHeistGameMode::AHeistGameMode()
{
	GameStateClass = AHeistGameState::StaticClass();

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
	//
	//   [주의] 서브시스템은 서버 것과 클라이언트 것이 서로 다른 객체다.
	//     호스트가 넣은 값은 참가자에게 가지 않는다. 여기서 읽는 것이 안전한 이유는
	//     이 함수가 서버 전용(GameMode)이기 때문이다. 클라이언트도 알아야 하는 값이
	//     생기면 서브시스템이 아니라 AHeistGameState 로 복제할 것.
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

void AHeistGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	AHeistGameState* GS = GetGameState<AHeistGameState>();
	if (!GS)
	{
		UE_LOG(LogHeist, Error,
			TEXT("GameState 가 AHeistGameState 가 아닙니다. 코어 루프가 돌지 않습니다 — "
				 "World Settings 의 GameState 클래스를 확인하세요."));
		return;
	}

	GS->SetTargetValue(TargetValue);

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

	// 판정은 다음 틱에 한다. 지금은 PlayerArray 에 나가는 사람이 아직 남아 있어
	// 생존자 수가 한 명 많게 세어진다 — 그 값으로 판정하면 끝날 판이 안 끝난다.
	GetWorldTimerManager().SetTimerForNextTick(this, &AHeistGameMode::TryFinishByEscape);
}

FHeistStartConditions AHeistGameMode::MakeStartConditions()
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	FHeistStartConditions Conditions;
	Conditions.NumPlayers = GetNumPlayers();
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

	EnterPhase(HHTags::Phase_Prep, EHeistPhaseReason::Scheduled);
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

	return 0.f;   // Result 는 결과 화면이 뜬 채로 머문다
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

		// 여기서 탈출 판정을 부르지 않는다. 본 작업이 시작되는 순간은 전원이 아직 밴
		// 근처에 있는 시점이라, 스폰 위치가 승차 볼륨과 조금이라도 겹치면 그 자리에서
		// $0 으로 판이 끝난다. 레벨 배치에 따라 되기도 하고 안 되기도 하는 사고다.
		//
		// 대신 판정은 '변화' 로만 성립하게 둔다 — 승차 · 하차 · 다운 · 접속 종료.
		// 준비 시간에 밴에 서 있던 사람도 명단에는 이미 올라와 있으므로,
		// 마지막 한 명이 타는 순간 그들까지 포함해 판정된다. 잃는 경우가 없다.
	}

	if (Phase == HHTags::Phase_Result)
	{
		ResolveArrests();
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
	TryFinishByEscape();
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
	// 다운으로 생존자가 줄면, 이미 밴에 타 있던 사람들만으로 전원 승차가 성립할 수 있다.
	// 복구로 생존자가 늘면 반대로 성립이 풀리는데, 그때는 아무 일도 일어나지 않는다
	TryFinishByEscape();
}

void AHeistGameMode::TryFinishByEscape()
{
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
		if (!IsValid(Player) || Player->IsOnlyASpectator() || Player->IsInactive())
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

void AHeistGameMode::HandlePhaseElapsed()
{
	// 타이머가 만료됐다는 것은 곧 "시간이 다 됐다" 는 뜻이다.
	// 경보로 인한 조기 진입은 이 경로가 아니라 AdvancePhase 로 들어온다
	AdvancePhase(EHeistPhaseReason::Scheduled);
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
