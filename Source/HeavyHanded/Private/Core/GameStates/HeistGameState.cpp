#include "Core/GameStates/HeistGameState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include "Alert/AlertComponent.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistLog.h"

AHeistGameState::AHeistGameState()
{
	// 남은 시간을 각자 계산하는 방식이라 서버 시계가 촘촘해야 한다.
	// 엔진 기본값과 같은 값이지만 명시해 둔다 — 누가 이 값을 늘리면
	// 페이즈 타이머 표시도 같이 거칠어진다는 사실이 여기 적혀 있어야 한다
	ServerWorldTimeSecondsUpdateFrequency = 0.1f;
}

void AHeistGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHeistGameState, CurrentPhase);
	DOREPLIFETIME(AHeistGameState, EntryTag);
	DOREPLIFETIME(AHeistGameState, PhaseEndServerTime);
	DOREPLIFETIME(AHeistGameState, PhaseReason);
	DOREPLIFETIME(AHeistGameState, LoadedValue);
	DOREPLIFETIME(AHeistGameState, TargetValue);
	DOREPLIFETIME(AHeistGameState, BoardedPlayers);
	DOREPLIFETIME(AHeistGameState, ArrestedPlayers);
	DOREPLIFETIME(AHeistGameState, LoadedEntries);
	DOREPLIFETIME(AHeistGameState, ElapsedSeconds);
	DOREPLIFETIME(AHeistGameState, Outcome);
	DOREPLIFETIME(AHeistGameState, ResultConfirmedPlayers);
}

AHeistGameState* AHeistGameState::Get(const UObject* WorldContext)
{
	if (!GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	// 작업 레벨이 아니면(로비 · 은신처) 다른 GameState 가 올라와 있다. 그때는 nullptr 이 정답이다
	return World->GetGameState<AHeistGameState>();
}

// ──────────────────────────────────────────────────────────────
// 페이즈
// ──────────────────────────────────────────────────────────────

bool AHeistGameState::TryGetPhaseRemainingSeconds(float& OutSeconds) const
{
	if (PhaseEndServerTime <= 0.f)
	{
		return false;   // 카운트다운 없음 — OutSeconds 는 건드리지 않는다
	}

	OutSeconds = FMath::Max(0.f, PhaseEndServerTime - GetServerWorldTimeSeconds());
	return true;
}

void AHeistGameState::SetPhase(const FGameplayTag& NewPhase, float DurationSeconds, EHeistPhaseReason Reason)
{
	if (!HasAuthority())
	{
		UE_LOG(LogHeist, Warning, TEXT("SetPhase(%s) 무시 — 클라이언트에서 호출되었습니다."),
			*NewPhase.ToString());
		return;
	}

	if (CurrentPhase == NewPhase)
	{
		UE_LOG(LogHeist, Warning, TEXT("SetPhase(%s) 무시 — 이미 그 페이즈입니다."),
			*NewPhase.ToString());
		return;
	}

	const FGameplayTag OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;
	PhaseReason = Reason;

	// 소요 시간은 페이즈 전이에 붙어 있는 사실이라 여기서 함께 기록한다.
	// 별도 함수로 빼면 GameMode 가 전이와 기록을 두 번 불러야 하고, 한쪽을 빠뜨린 경로가
	// 생기면 결과 화면에 0초가 찍힌다 — 그때 원인은 코드를 봐야만 알 수 있다.
	if (NewPhase == HHTags::Phase_Heist)
	{
		HeistStartServerTime = GetServerWorldTimeSeconds();
	}
	else if (NewPhase == HHTags::Phase_Result)
	{
		// 본 작업에 들어가 본 적이 없으면(치트로 건너뛴 판) 기준점이 없다. 0 으로 둔다
		ElapsedSeconds = (HeistStartServerTime > 0.f)
			? FMath::Max(0.f, GetServerWorldTimeSeconds() - HeistStartServerTime)
			: 0.f;
	}

	PhaseEndServerTime = (DurationSeconds > 0.f)
		? GetServerWorldTimeSeconds() + DurationSeconds
		: 0.f;

	UE_LOG(LogHeist, Log, TEXT("페이즈 전환 %s → %s (%s, 사유: %s)"),
		OldPhase.IsValid() ? *OldPhase.ToString() : TEXT("(없음)"),
		*NewPhase.ToString(),
		(DurationSeconds > 0.f) ? *FString::Printf(TEXT("%.0f초"), DurationSeconds) : TEXT("카운트다운 없음"),
		HeistPhase::ToString(Reason));

	// 서버에서는 RepNotify 가 자동으로 불리지 않는다. 구독자가 서버 · 클라 어디에 있든
	// 같은 시점에 같은 값을 받게 하려면 여기서 직접 불러 준다
	OnRep_CurrentPhase(OldPhase);
}

void AHeistGameState::OnRep_CurrentPhase(FGameplayTag OldPhase)
{
	OnPhaseChanged.Broadcast(CurrentPhase, OldPhase, PhaseReason);
}

// ──────────────────────────────────────────────────────────────
// 목표 금액
// ──────────────────────────────────────────────────────────────

void AHeistGameState::AddLoadedValue(int32 DeltaValue)
{
	if (!HasAuthority() || DeltaValue == 0)
	{
		return;
	}

	LoadedValue = FMath::Max(0, LoadedValue + DeltaValue);

	UE_LOG(LogHeist, Log, TEXT("적재 금액 %+d → $%d / $%d"), DeltaValue, LoadedValue, TargetValue);

	OnRep_LoadedValue();
}

void AHeistGameState::RecordLoadedLoot(const FHeistLoadEntry& Entry)
{
	if (!HasAuthority())
	{
		return;
	}

	// 금액보다 목록이 먼저다. 파괴돼서 가치가 0 이 된 노획물은 AddLoadedValue 가 그냥 빠지는데,
	// 그래도 "무엇을 실었는가" 에는 남아야 한다 — 깨진 채로 실어 온 것도 결과 화면의 사실이다
	LoadedEntries.Add(Entry);

	AddLoadedValue(Entry.Value);
}

void AHeistGameState::SetTargetValue(int32 NewTargetValue)
{
	if (!HasAuthority())
	{
		return;
	}

	TargetValue = FMath::Max(0, NewTargetValue);

	// 목표가 바뀌면 게이지의 분모가 바뀐다. 적재액이 그대로여도 HUD 는 다시 그려야 한다
	OnRep_LoadedValue();
}

void AHeistGameState::SetEntryTag(const FGameplayTag& NewEntryTag)
{
	if (!HasAuthority())
	{
		return;
	}

	EntryTag = NewEntryTag;
}

void AHeistGameState::OnRep_LoadedValue()
{
	OnLoadedValueChanged.Broadcast(LoadedValue, TargetValue);
}

// ──────────────────────────────────────────────────────────────
// 결과
// ──────────────────────────────────────────────────────────────

void AHeistGameState::FinalizeOutcome()
{
	if (!HasAuthority())
	{
		return;
	}

	// 탈출은 '체포되지 않았다' 로 본다. 체포는 미승차와 다운을 모두 흡수한 결과라
	// 여기서 그 조건을 다시 세면 두 곳이 어긋날 수 있다.
	int32 CountedNum = 0;
	int32 EscapedNum = 0;

	for (const APlayerState* Player : PlayerArray)
	{
		if (!IsCountedPlayer(Player))
		{
			continue;
		}

		++CountedNum;

		if (!IsArrested(Player))
		{
			++EscapedNum;
		}
	}

	// 최소 1인이 빠져나오면 작업은 성립한다 (기획서 2장 승패 조건).
	//
	// 아무도 남지 않은 판(전원 이탈)을 성공으로 읽는 함정은 이 조건 자체가 막는다 —
	// 셀 사람이 없으면 EscapedNum 이 0 이라 그대로 실패로 떨어진다. '전원 탈출' 로 세던
	// 시절에는 0명 중 0명이 참이라 CountedNum > 0 가드를 따로 세워야 했다.
	const bool bAnyoneEscaped = EscapedNum > 0;

	Outcome = HeistOutcome::Evaluate(IsTargetReached(), bAnyoneEscaped);

	UE_LOG(LogHeist, Log, TEXT("결과 등급 %s — 적재 $%d / $%d, 탈출 %d of %d명, 소요 %.1f초"),
		HeistOutcome::ToString(Outcome),
		LoadedValue, TargetValue, EscapedNum, CountedNum, ElapsedSeconds);
}

int32 AHeistGameState::GetContributionOf(const APlayerState* Player) const
{
	if (!IsValid(Player))
	{
		return 0;
	}

	int32 Total = 0;

	for (const FHeistLoadEntry& Entry : LoadedEntries)
	{
		if (Entry.Loader == Player)
		{
			Total += Entry.Value;
		}
	}

	return Total;
}

void AHeistGameState::SetResultConfirmed(APlayerState* Player, bool bConfirmed)
{
	if (!HasAuthority() || !IsValid(Player))
	{
		return;
	}

	// Result 에서만 받는다. 승차 명단이 Result 에서만 잠기는 것과 짝이다.
	//
	// 그전에 들어온 확인이 명단에 쌓이면 Result 진입 순간에는 값이 안 바뀌어
	// OnResultConfirmChanged 가 울리지 않는다. 전원이 이미 확인한 상태인데 아무도
	// 그 사실을 모르고, 체류 시간이 다 될 때까지 결과 화면에 갇힌다.
	// 지금은 확인을 넣을 UI 경로가 없지만 세션·UI 파트의 Server RPC 가 붙는 순간 열린다.
	if (!IsPhase(HHTags::Phase_Result))
	{
		UE_LOG(LogHeist, Verbose, TEXT("결과 확인 무시 — 아직 결과 화면이 아니다 (%s)"),
			*Player->GetPlayerName());
		return;
	}

	const bool bWasConfirmed = ResultConfirmedPlayers.Contains(Player);
	if (bWasConfirmed == bConfirmed)
	{
		return;   // 같은 상태를 다시 넣지 않는다
	}

	if (bConfirmed)
	{
		ResultConfirmedPlayers.Add(Player);
	}
	else
	{
		ResultConfirmedPlayers.Remove(Player);
	}

	UE_LOG(LogHeist, Log, TEXT("결과 확인 %s — %s (%d명)"),
		bConfirmed ? TEXT("완료") : TEXT("취소"),
		*Player->GetPlayerName(), ResultConfirmedPlayers.Num());

	// 서버에서는 RepNotify 가 자동으로 불리지 않는다. SetPhase 와 같은 이유다
	OnRep_ResultConfirmedPlayers();
}

bool AHeistGameState::IsResultConfirmed(const APlayerState* Player) const
{
	return IsValid(Player) && ResultConfirmedPlayers.Contains(Player);
}

bool AHeistGameState::AreAllResultsConfirmed() const
{
	int32 CountedNum = 0;
	int32 ConfirmedNum = 0;

	for (const APlayerState* Player : PlayerArray)
	{
		if (!IsCountedPlayer(Player))
		{
			continue;
		}

		++CountedNum;

		if (ResultConfirmedPlayers.Contains(Player))
		{
			++ConfirmedNum;
		}
	}

	// 아무도 없는 판을 "전원 확인" 으로 읽으면 빈 서버가 스스로 매치를 끝낸다
	return CountedNum > 0 && ConfirmedNum >= CountedNum;
}

void AHeistGameState::OnRep_ResultConfirmedPlayers()
{
	int32 CountedNum = 0;
	for (const APlayerState* Player : PlayerArray)
	{
		if (IsCountedPlayer(Player))
		{
			++CountedNum;
		}
	}

	OnResultConfirmChanged.Broadcast(ResultConfirmedPlayers.Num(), CountedNum);
}

APlayerState* AHeistGameState::GetNoisiestPlayer(float& OutContribution) const
{
	OutContribution = 0.f;

	// 작업 레벨이 아닌 테스트 맵에는 경계도 컴포넌트가 없을 수 있다.
	// 그때는 "아무도 없음" 이 맞는 답이다 — 소음을 세는 주체가 없었다는 뜻이므로
	const UAlertComponent* Alert = UAlertComponent::Get(this);

	return Alert ? Alert->GetNoisiestPlayer(OutContribution) : nullptr;
}

// ──────────────────────────────────────────────────────────────
// 탈출
// ──────────────────────────────────────────────────────────────

bool AHeistGameState::IsCountedPlayer(const APlayerState* Player)
{
	// IsInactive 는 접속이 끊긴 뒤 재접속을 위해 남겨 둔 껍데기다. 이걸 안 걸면
	// 나간 사람이 영영 "안 탄 사람" 으로 남아 전원 승차가 성립하지 않는다.
	return IsValid(Player) && !Player->IsOnlyASpectator() && !Player->IsInactive();
}

bool AHeistGameState::IsPlayerDowned(const APlayerState* Player)
{
	const IAbilitySystemInterface* AsAbilitySystem = Cast<IAbilitySystemInterface>(Player);
	if (!AsAbilitySystem)
	{
		return false;
	}

	// 다운 시스템(전영배)이 아직 이 태그를 붙이지 않는다. 그래서 지금은 언제나 false 이고,
	// 다운 관련 분기가 전부 잠들어 있다 — GE 가 들어오는 순간 그대로 살아난다.
	const UAbilitySystemComponent* ASC = AsAbilitySystem->GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(HHTags::State_Downed);
}

void AHeistGameState::SetMirrorTag(APlayerState* Player, const FGameplayTag& Tag, bool bApply)
{
	IAbilitySystemInterface* AsAbilitySystem = Cast<IAbilitySystemInterface>(Player);
	if (!AsAbilitySystem)
	{
		return;
	}

	UAbilitySystemComponent* ASC = AsAbilitySystem->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Loose 태그를 쓰는 이유는 이 상태의 주인이 GAS 가 아니기 때문이다. 진리원은 명단이고
	// 태그는 그 사본이라, 태그를 만드는 GameplayEffect 를 따로 두면 주인이 둘이 된다.
	//
	// 복제판(AddReplicated~)인 이유는 이 태그의 유일한 용도가 남에게 보이는 것이라서다.
	// 그냥 AddLooseGameplayTag 는 서버에만 붙어서, 정작 이걸 읽어야 할 클라이언트 HUD 와
	// 어빌리티 차단 판정에는 아무것도 도착하지 않는다.
	if (bApply)
	{
		ASC->AddReplicatedLooseGameplayTag(Tag);
	}
	else
	{
		ASC->RemoveReplicatedLooseGameplayTag(Tag);
	}
}

bool AHeistGameState::IsBoarded(const APlayerState* Player) const
{
	return IsValid(Player) && BoardedPlayers.Contains(Player);
}

bool AHeistGameState::IsArrested(const APlayerState* Player) const
{
	return IsValid(Player) && ArrestedPlayers.Contains(Player);
}

int32 AHeistGameState::GetSurvivorNum() const
{
	return HeistEscapeGate::GetSurvivorNum(MakeEscapeConditions());
}

FHeistEscapeConditions AHeistGameState::MakeEscapeConditions() const
{
	FHeistEscapeConditions Conditions;

	for (const APlayerState* Player : PlayerArray)
	{
		if (!IsCountedPlayer(Player))
		{
			continue;
		}

		++Conditions.NumActivePlayers;

		if (IsPlayerDowned(Player))
		{
			++Conditions.NumDownedPlayers;
			continue;   // 다운자는 밴에 있어도 탈출 인원이 아니다
		}

		if (BoardedPlayers.Contains(Player))
		{
			++Conditions.NumBoardedSurvivors;
		}
	}

	return Conditions;
}

void AHeistGameState::SetBoarded(APlayerState* Player, bool bBoarded)
{
	if (!HasAuthority() || !IsValid(Player))
	{
		return;
	}

	// Result 에 들어간 뒤로는 명단이 얼어야 한다.
	//
	// 체포는 ResolveArrests 가 이미 확정했고 등급도 그 위에서 나왔다. 여기서 승차만 더
	// 바뀌면 탈출 명단에도 체포 명단에도 없는 사람이 생겨, 결과 화면이 자기가 표시하는
	// 등급과 어긋난 명단을 그린다.
	//
	// 계기가 둘이라 이 자리에서 막는다 — 결과 화면에서의 하차 상호작용(AVanZone)과
	// 접속 종료(AHeistGameMode::Logout). 부르는 쪽에서 각각 막으면 반드시 한쪽이 빠진다.
	if (IsPhase(HHTags::Phase_Result))
	{
		UE_LOG(LogHeist, Verbose, TEXT("승차 명단 변경 무시 — 이미 결과가 확정된 판이다 (%s)"),
			*Player->GetPlayerName());
		return;
	}

	const bool bWasBoarded = BoardedPlayers.Contains(Player);
	if (bWasBoarded == bBoarded)
	{
		return;   // 같은 상태를 다시 넣지 않는다 — 복제와 방송을 헛되이 일으킬 이유가 없다
	}

	if (bBoarded)
	{
		BoardedPlayers.Add(Player);
	}
	else
	{
		BoardedPlayers.Remove(Player);
	}

	SetMirrorTag(Player, HHTags::State_InVan, bBoarded);

	UE_LOG(LogHeist, Log, TEXT("%s %s — 승차 %d명 / 생존 %d명"),
		*Player->GetPlayerName(), bBoarded ? TEXT("승차") : TEXT("하차"),
		BoardedPlayers.Num(), GetSurvivorNum());

	// 서버에서는 RepNotify 가 자동으로 불리지 않는다. 구독자가 어디에 있든 같은 시점에
	// 같은 값을 받게 하려면 여기서 직접 불러 준다 — SetPhase 와 같은 이유다
	BroadcastBoardedChanged();
}

void AHeistGameState::MarkArrested(APlayerState* Player)
{
	if (!HasAuthority() || !IsValid(Player) || ArrestedPlayers.Contains(Player))
	{
		return;
	}

	ArrestedPlayers.Add(Player);
	SetMirrorTag(Player, HHTags::State_Arrested, true);

	UE_LOG(LogHeist, Log, TEXT("체포 — %s"), *Player->GetPlayerName());
}

void AHeistGameState::OnRep_BoardedPlayers()
{
	BroadcastBoardedChanged();
}

void AHeistGameState::BroadcastBoardedChanged()
{
	OnBoardedChanged.Broadcast(BoardedPlayers.Num(), GetSurvivorNum());
}
