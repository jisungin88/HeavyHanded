#include "Core/PlayerControllers/HeistPlayerController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistLog.h"

namespace
{
	/**
	 * 관전 상태를 다시 보는 주기(초).
	 *
	 * 밸런싱 값이 아니라 구현 상수라 UHeistSettings 에 두지 않는다 —
	 * AHeistGameMode 의 StartWaitPollSeconds 와 같은 성격이다.
	 */
	constexpr float SpectatePollSeconds = 0.5f;
}

void AHeistPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		// 두 경로가 다 필요하다 — 호스트와 늦게 온 클라이언트는 GameState 도착 시점이 다르다.
		// 이미 와 있으면 지금 붙고, 아직이면 도착할 때 붙는다
		BindToGameState(World->GetGameState());

		GameStateSetHandle =
			World->GameStateSetEvent.AddUObject(this, &AHeistPlayerController::BindToGameState);

		// 관전 판정은 내 PlayerState 가 와야 알 수 있고, 그건 지금 없을 수 있다.
		// 로컬 컨트롤러만 돈다 — 시점은 자기 화면의 문제다
		if (IsLocalController())
		{
			World->GetTimerManager().SetTimer(SpectateTimer, this,
				&AHeistPlayerController::TickSpectate, SpectatePollSeconds, true);
		}
	}

	// 접속 대기 중에도 화면은 떠 있어야 한다 — 지금 무엇을 기다리는지 보여줄 곳이 HUD 뿐이다.
	// 로컬 컨트롤러가 아니면 베이스가 걸러낸다
	EnsureHUDWidget();
}

void AHeistPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromGameState();

	if (UWorld* World = GetWorld())
	{
		if (GameStateSetHandle.IsValid())
		{
			World->GameStateSetEvent.Remove(GameStateSetHandle);
			GameStateSetHandle.Reset();
		}

		World->GetTimerManager().ClearTimer(SpectateTimer);
	}

	Super::EndPlay(EndPlayReason);
}

// ──────────────────────────────────────────────────────────────
// 페이즈
// ──────────────────────────────────────────────────────────────

void AHeistPlayerController::BindToGameState(AGameStateBase* GameState)
{
	AHeistGameState* HeistState = Cast<AHeistGameState>(GameState);

	// 작업 레벨이 아니면(테스트 맵 등) 그냥 아무것도 하지 않는다.
	// 같은 GameState 로 두 번 불리는 것도 정상 경로다 — BeginPlay 와 GameStateSetEvent 가
	// 겹칠 수 있어서, 여기서 걸러내지 않으면 델리게이트가 두 번 붙는다
	if (!HeistState || HeistState == BoundGameState)
	{
		return;
	}

	UnbindFromGameState();

	BoundGameState = HeistState;
	HeistState->OnPhaseChanged.AddDynamic(this, &AHeistPlayerController::HandlePhaseChanged);

	// 이미 페이즈가 진행 중일 수 있다. 늦게 들어온 클라이언트는 지나간 전환 알림을 못 받으므로
	// 지금 값으로 한 번 맞춰 두지 않으면 **다음 전환이 올 때까지** 화면이 어긋난 채로 있다.
	// 접속 대기 중에는 페이즈가 비어 있어서 여기로 오지 않는다
	const FGameplayTag CurrentPhase = HeistState->GetCurrentPhase();
	if (CurrentPhase.IsValid())
	{
		HandlePhaseChanged(CurrentPhase, FGameplayTag(), HeistState->GetPhaseReason());
	}
}

void AHeistPlayerController::UnbindFromGameState()
{
	if (!BoundGameState)
	{
		return;
	}

	BoundGameState->OnPhaseChanged.RemoveDynamic(this, &AHeistPlayerController::HandlePhaseChanged);
	BoundGameState = nullptr;
}

void AHeistPlayerController::HandlePhaseChanged(
	FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason)
{
	UE_LOG(LogHeist, Verbose, TEXT("[PC] 페이즈 %s → %s (%s)"),
		*OldPhase.ToString(), *NewPhase.ToString(), HeistPhase::ToString(Reason));

	// 결과 화면만 게임 입력을 막는다. 판이 이미 끝났고 남은 조작은 '확인' 하나뿐이다.
	// 그 외 페이즈에서는 UI 가 떠 있어도 걸어 다닐 수 있어야 한다 — 준비 시간의 장비 창을
	// 열어 둔 채로 이동하지 못하면 45초 안에 아무것도 못 한다.
	//
	// 소유자로 this 를 넘기는 이유: 이 포커스는 페이즈가 쥐고 있는 것이라, 은신처 단말기처럼
	// 플레이어가 닫을 수 있는 UI 가 실수로 풀어 버리면 안 된다
	if (NewPhase.MatchesTag(HHTags::Phase_Result))
	{
		EnterUIFocus(this, EHHUIFocusMode::UIOnly);
	}
	else
	{
		ExitUIFocus(this);
	}
}

// ──────────────────────────────────────────────────────────────
// 결과 확인
// ──────────────────────────────────────────────────────────────

bool AHeistPlayerController::Server_SetResultConfirmed_Validate(bool /*bConfirmed*/)
{
	// bool 에는 막을 범위가 없다. 그래도 WithValidation 을 붙여 두는 것은 문서 02 의 규칙이고,
	// 인자가 늘어날 때 검증이 들어갈 자리를 미리 만들어 두는 것이기도 하다.
	//
	// "결과 화면이 아닐 때 눌렀다" 같은 정상 범위의 잘못된 입력은 여기서 막지 않는다 —
	// _Validate 실패는 엔진이 그 클라이언트의 **접속을 끊는** 동작이다. _Validate 는
	// 악의적인 입력만 막고, 나머지는 아래에서 조용히 무시한다
	return true;
}

void AHeistPlayerController::Server_SetResultConfirmed_Implementation(bool bConfirmed)
{
	AHeistGameState* HeistState = AHeistGameState::Get(this);
	if (!HeistState || !PlayerState)
	{
		return;
	}

	// 결과 화면이 아닐 때 들어온 확인은 조용히 무시한다. 클라이언트 HUD 가 한 프레임 늦게
	// 닫히면서 보낼 수 있는 정상적인 경로라, 끊거나 경고할 일이 아니다
	if (!HeistState->IsPhase(HHTags::Phase_Result))
	{
		return;
	}

	HeistState->SetResultConfirmed(PlayerState, bConfirmed);
}

// ──────────────────────────────────────────────────────────────
// 관전
// ──────────────────────────────────────────────────────────────

bool AHeistPlayerController::IsSpectating() const
{
	return PlayerState && PlayerState->IsOnlyASpectator();
}

void AHeistPlayerController::TickSpectate()
{
	// 아직 안 왔다. 다음 주기에 다시 본다
	if (!PlayerState)
	{
		return;
	}

	if (!IsSpectating())
	{
		// 관전자가 아니다. 체포는 레벨 진입 시점에 정해지고 판 중에 바뀌지 않으므로
		// 다시 볼 이유가 없다 — 여기서 타이머를 끈다
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SpectateTimer);
		}
		return;
	}

	EnsureSpectateTarget();
}

void AHeistPlayerController::EnsureSpectateTarget()
{
	AActor* Current = GetViewTarget();

	// 자기 자신을 보고 있으면 대상이 없는 것이다 — 관전 폰이 없으니 화면에 아무것도 안 나온다.
	// 대상이 파괴된 경우도 엔진이 여기로 되돌려 놓는다 (APlayerCameraManager::AssignViewTarget)
	const bool bNeedsTarget = !IsValid(Current) || Current == this;
	if (!bNeedsTarget)
	{
		return;
	}

	if (AActor* Target = PickSpectateTarget(0))
	{
		SetViewTarget(Target);
		UE_LOG(LogHeist, Verbose, TEXT("[관전] 시점을 %s 로 옮겼습니다."), *GetNameSafe(Target));
	}

	// 아무도 없으면 그대로 둔다. 팀원 폰이 아직 복제되지 않았을 뿐일 수 있어서
	// 다음 주기에 다시 시도한다 — 여기서 로그를 남기면 초당 두 줄씩 쌓인다
}

void AHeistPlayerController::ViewNextTeammate()
{
	if (!IsSpectating())
	{
		return;
	}

	if (AActor* Target = PickSpectateTarget(1))
	{
		SetViewTarget(Target);
	}
}

void AHeistPlayerController::ViewPreviousTeammate()
{
	if (!IsSpectating())
	{
		return;
	}

	if (AActor* Target = PickSpectateTarget(-1))
	{
		SetViewTarget(Target);
	}
}

AActor* AHeistPlayerController::PickSpectateTarget(int32 Step) const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return nullptr;
	}

	// 볼 수 있는 사람 = 이 판에 실제로 뛰고 있는 사람. IsCountedPlayer 를 쓰면 다른 관전자와
	// 접속이 끊긴 껍데기가 한 번에 걸러진다 — 여기서 조건을 다시 적으면 승차 · 체포 판정과
	// 모집단이 갈라지고, 그때 "명단에 없는 사람을 보고 있다" 가 된다
	TArray<APawn*> Candidates;

	for (const APlayerState* Candidate : GS->PlayerArray)
	{
		if (Candidate == PlayerState || !AHeistGameState::IsCountedPlayer(Candidate))
		{
			continue;
		}

		if (APawn* CandidatePawn = Candidate->GetPawn())
		{
			Candidates.Add(CandidatePawn);
		}
	}

	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	// 현재 대상이 목록에 있으면 거기서 이동하고, 없으면(파괴됐거나 첫 진입) 처음부터 본다.
	// Step 0 은 "지금 것을 유지" 라서 현재 대상이 유효하면 그대로 돌아간다
	const int32 CurrentIndex = Candidates.IndexOfByKey(Cast<APawn>(GetViewTarget()));
	if (CurrentIndex == INDEX_NONE)
	{
		return Candidates[0];
	}

	// 음수 나머지를 피하려고 크기를 한 번 더한다 — Step 이 -1 일 때 인덱스가 -1 이 되면
	// 배열 접근에서 그대로 터진다
	const int32 NextIndex = (CurrentIndex + Step + Candidates.Num()) % Candidates.Num();
	return Candidates[NextIndex];
}
