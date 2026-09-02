#include "Core/PlayerControllers/HeistPlayerController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistLog.h"
#include "Core/HeistSettings.h"
#include "Core/Spectate/HeistSpectatorComponent.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"

AHeistPlayerController::AHeistPlayerController()
{
	SpectatorComponent = CreateDefaultSubobject<UHeistSpectatorComponent>(TEXT("SpectatorComponent"));
}

void AHeistPlayerController::Server_SetSpectateTarget_Implementation(APlayerState* Target)
{
	UE_LOG(LogHeist, Log, TEXT("[관전RPC] Target=%s / 나는관전자=%d / IsCounted=%d / 대상폰=%s"),
			  *GetNameSafe(Target),
			  PlayerState ? (int32)PlayerState->IsOnlyASpectator() : -1,
			  (int32)AHeistGameState::IsCountedPlayer(Target),
			  Target ? *GetNameSafe(Target->GetPawn()) : TEXT("-"));

	if (!PlayerState || !PlayerState->IsOnlyASpectator())
	{
		return;
	}

	if (!AHeistGameState::IsCountedPlayer(Target))
	{
		return;
	}

	if (APawn* TargetPawn = Target->GetPawn())
	{
		SetViewTarget(TargetPawn);
	}
}

bool AHeistPlayerController::Server_SetSpectateTarget_Validate(APlayerState* Target)
{
	return true;
}

void AHeistPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}

	const UHeistSettings* Settings = UHeistSettings::Get();

	if (UInputAction* NextAction = Settings->SpectateNextAction.LoadSynchronous())
	{
		EIC->BindAction(NextAction, ETriggerEvent::Started, this, &AHeistPlayerController::OnSpectateNext);
	}
	if (UInputAction* PrevAction = Settings->SpectatePrevAction.LoadSynchronous())
	{
		EIC->BindAction(PrevAction, ETriggerEvent::Started, this, &AHeistPlayerController::OnSpectatePrev);
	}
}

void AHeistPlayerController::OnSpectateNext()
{
	if (SpectatorComponent)
	{
		SpectatorComponent->ViewNext();
	}
}

void AHeistPlayerController::OnSpectatePrev()
{
	if (SpectatorComponent)
	{
		SpectatorComponent->ViewPrevious();
	}
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
	HeistState->OnStartWaitChanged.AddDynamic(this, &AHeistPlayerController::HandleStartWaitChanged);

	// 접속 대기는 페이즈보다 먼저 시작되므로, 늦게 붙은 컨트롤러는 이미 지나간 알림을 못 받는다.
	// 지금 값으로 한 번 맞추지 않으면 **입력이 막히지 않은 채** 대기 구간을 보낸다
	HandleStartWaitChanged(HeistState->GetStartWaitState());

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
	BoundGameState->OnStartWaitChanged.RemoveDynamic(this, &AHeistPlayerController::HandleStartWaitChanged);
	BoundGameState = nullptr;
}

void AHeistPlayerController::HandleStartWaitChanged(FHeistStartWaitState State)
{
	if (State.bWaiting)
	{
		// 결과 화면과 같은 차단이다 — 판이 아직 시작되지 않았으므로 게임 입력이 들어가면 안 된다.
		//
		// 소유자로 this 를 넘기는 이유는 HandlePhaseChanged 와 같다. 대기 해제와 Phase.Prep 은
		// 둘 다 ExitUIFocus(this) 로 끝나므로 어느 쪽이 먼저 와도 결과가 같다
		EnterUIFocus(this, EHHUIFocusMode::UIOnly);
	}
	else
	{
		ExitUIFocus(this);
	}

	OnStartWaitChanged(State.bWaiting, State.NumConnected, State.NumExpected);
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

void AHeistPlayerController::DumpSpectateCamera() const
{
	if (!PlayerCameraManager)
	{
		UE_LOG(LogHeist, Warning, TEXT("[관전카메라] PlayerCameraManager 가 없습니다."));
		return;
	}

	const APawn* VTPawn = PlayerCameraManager->GetViewTargetPawn();

	UE_LOG(LogHeist, Log, TEXT("[관전카메라] ViewTarget=%s / Pawn=%s / Role=%d / 폰의Controller=%s"),
			*GetNameSafe(PlayerCameraManager->GetViewTarget()),
			*GetNameSafe(VTPawn),
			VTPawn ? (int32)VTPawn->GetLocalRole() : -1,
			VTPawn ? *GetNameSafe(VTPawn->GetController()) : TEXT("-"));

	UE_LOG(LogHeist, Log, TEXT("[관전카메라] Target=%s / Blended=%s / 폰의GetViewRotation=%s"),
			*TargetViewRotation.ToCompactString(),
			*BlendedTargetViewRotation.ToCompactString(),
			VTPawn ? *VTPawn->GetViewRotation().ToCompactString() : TEXT("-"));
}
