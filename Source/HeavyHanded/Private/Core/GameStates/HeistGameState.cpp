#include "Core/GameStates/HeistGameState.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

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
	DOREPLIFETIME(AHeistGameState, PhaseEndServerTime);
	DOREPLIFETIME(AHeistGameState, PhaseReason);
	DOREPLIFETIME(AHeistGameState, LoadedValue);
	DOREPLIFETIME(AHeistGameState, TargetValue);
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

void AHeistGameState::OnRep_LoadedValue()
{
	OnLoadedValueChanged.Broadcast(LoadedValue, TargetValue);
}
