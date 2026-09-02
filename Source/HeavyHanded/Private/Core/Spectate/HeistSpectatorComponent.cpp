#include "Core/Spectate/HeistSpectatorComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeistLog.h"
#include "Core/HeistSettings.h"
#include "Core/PlayerControllers/HeistPlayerController.h"

#include "HAL/IConsoleManager.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

namespace
{
	/** 관전 상태를 다시 보는 주기(초) */
	constexpr float SpectatePollSeconds = 1.f;
}

UHeistSpectatorComponent::UHeistSpectatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeistSpectatorComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PC = GetOwningPC();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// 관전 판정은 PlayerState가 와야 알 수 있
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SpectateTimer, this,
			&UHeistSpectatorComponent::TickSpectate, SpectatePollSeconds, true);
	}
}

void UHeistSpectatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ApplySpectateInput(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpectateTimer);
	}

	Super::EndPlay(EndPlayReason);
}

APlayerController* UHeistSpectatorComponent::GetOwningPC() const
{
	return Cast<APlayerController>(GetOwner());
}

EHeistSpectateInfoLevel UHeistSpectatorComponent::GetInfoLevel() const
{
	return UHeistSettings::Get()->SpectateInfoLevel;
}

// --------------------------
// 관전
// --------------------------

bool UHeistSpectatorComponent::IsSpectating() const
{
	const APlayerController* PC = GetOwningPC();
	return PC && PC->PlayerState && PC->PlayerState->IsOnlyASpectator();
}

void UHeistSpectatorComponent::TickSpectate()
{
	const APlayerController* PC = GetOwningPC();
	if (!PC || !PC->PlayerState)
	{
		return;
	}

	if (!IsSpectating())
	{
		ApplySpectateInput(false);
		return;
	}

	ApplySpectateInput(true);
	EnsureTarget();
}

void UHeistSpectatorComponent::EnsureTarget()
{
	APlayerController* PC = GetOwningPC();
	if (!PC)
	{
		return;
	}

	AActor* Current = PC->GetViewTarget();
	if (IsValid(Current) && Current != PC)
	{
		return;
	}

	ApplyViewTarget(PickTarget(0));
}

void UHeistSpectatorComponent::ViewNext()
{
	ViewStep(1);
}

void UHeistSpectatorComponent::ViewPrevious()
{
	ViewStep(-1);
}

void UHeistSpectatorComponent::ViewStep(int32 Step)
{
	if (!IsSpectating())
	{
		return;
	}

	ApplyViewTarget(PickTarget(Step));
}

void UHeistSpectatorComponent::ApplySpectateInput(bool bEnable)
{
	if (bEnable == bSpectateInputApplied)
	{
		return;
	}

	APlayerController* PC = GetOwningPC();
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LP ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;
	if (!Subsystem)
	{
		return;
	}

	UInputMappingContext* IMC = UHeistSettings::Get()->SpectateMappingContext.LoadSynchronous();
	if (!IMC)
	{
		// 조용히 넘어가면 "관전인데 키가 안 먹는다" 로만 드러난다 — 한 번 알린다
		UE_LOG(LogHeist, Warning,
				TEXT("[관전] SpectateMappingContext 가 비어 있어 관전 입력을 붙이지 못했습니다. "
					 "Project Settings > Game > Heist > Spectate 에서 지정하세요."));
		return;
	}

	if (bEnable)
	{
		Subsystem->AddMappingContext(IMC, 100);
	}
	else
	{
		Subsystem->RemoveMappingContext(IMC);
	}

	bSpectateInputApplied = bEnable;
}

APlayerState* UHeistSpectatorComponent::GetViewedPlayer() const
{
	return ViewedPlayer.Get();
}

void UHeistSpectatorComponent::BuildCandidates(TArray<APlayerState*>& Out) const
{
	Out.Reset();

	const APlayerController* PC = GetOwningPC();
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!PC || !GS)
	{
		return;
	}

	for (APlayerState* Candidate : GS->PlayerArray)
	{
		if (Candidate == PC->PlayerState || !AHeistGameState::IsCountedPlayer(Candidate))
		{
			continue;
		}

		// 폰이 없어도 반영함 - 목록 길이 유지
		Out.Add(Candidate);
	}

	// 순서 정렬 - 클라&호스트 순서 일치
	Out.Sort([](const APlayerState& A, const APlayerState& B)
	{
		return A.GetPlayerId() < B.GetPlayerId();
	});
}

APlayerState* UHeistSpectatorComponent::PickTarget(int32 Step) const
{
	TArray<APlayerState*> Candidates;
	BuildCandidates(Candidates);

	const int32 Num = Candidates.Num();
	if (Num == 0)
	{
		return nullptr;
	}

	const int32 Cursor = Candidates.IndexOfByKey(ViewedPlayer.Get());

	// 커서가 없을 때 첫 번째
	int32 Index = (Cursor == INDEX_NONE) ? 0 : (Cursor + Step + Num) % Num;

	// 폰이 아직 복제되지 않은 사람은 건너뜀
	const int32 Dir = (Step < 0) ? -1 : 1;
	for (int32 Tried = 0; Tried < Num; ++Tried)
	{
		if (Candidates[Index]->GetPawn())
		{
			return Candidates[Index];
		}

		Index = (Index + Dir + Num) % Num;
	}

	return nullptr;
}

void UHeistSpectatorComponent::ApplyViewTarget(APlayerState* Target)
{
	APlayerController* PC = GetOwningPC();
	if (!PC || !Target)
	{
		return;
	}

	APawn* TargetPawn = Target->GetPawn();
	if (!TargetPawn)
	{
		return;
	}

	PC->SetViewTarget(TargetPawn);
	ViewedPlayer = Target;

	// 서버
	if (AHeistPlayerController* HeistPC = Cast<AHeistPlayerController>(PC))
	{
		HeistPC->Server_SetSpectateTarget(Target);
	}

	UE_LOG(LogHeist, Verbose, TEXT("[관전] 시점 -> %s"), *Target->GetPlayerName());
}

// -----------------------
// 치트
// ------------------------

void UHeistSpectatorComponent::DumpSpectateState() const
{
	TArray<APlayerState*> Candidates;
	BuildCandidates(Candidates);

	UE_LOG(LogHeist, Log, TEXT("- 관전 - 상태 %s / 대상 %s / 후보 %d명"),
			  IsSpectating() ? TEXT("관전") : TEXT("플레이"),
			  ViewedPlayer.IsValid() ? *ViewedPlayer->GetPlayerName() : TEXT("(없음)"),
			  Candidates.Num());

	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		UE_LOG(LogHeist, Log, TEXT("  [%d] Id=%d %s%s"),
					  i,
					  Candidates[i]->GetPlayerId(),
					  *Candidates[i]->GetPlayerName(),
					  Candidates[i]->GetPawn() ? TEXT("") : TEXT(" (폰 없음)"));
	}

	if (const AHeistPlayerController* HeistPC = Cast<AHeistPlayerController>(GetOwningPC()))
	{
		HeistPC->DumpSpectateCamera();
	}
}

static void SpectateShowCommand(UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogHeist, Warning, TEXT("로컬 PlayerController 가 없습니다."));
		return;
	}

	const UHeistSpectatorComponent* Spectator = PC->FindComponentByClass<UHeistSpectatorComponent>();
	if (!Spectator)
	{
		UE_LOG(LogHeist, Warning, TEXT("작업 레벨이 아닙니다 — 관전 컴포넌트가 없습니다."));
		return;
	}

	Spectator->DumpSpectateState();
}

static FAutoConsoleCommandWithWorld GSpectateShowCommand(
		TEXT("hh.Spectate.Show"),
		TEXT("관전 상태와 후보 목록을 찍는다. 클라이언트 창에서도 동작한다"),
		FConsoleCommandWithWorldDelegate::CreateStatic(&SpectateShowCommand),
		ECVF_Cheat);
