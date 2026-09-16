#include "UI/Shelter/ShelterHUD.h"

#include "Blueprint/UserWidget.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/HeavyUILog.h"
#include "UI/Shelter/ChatBoxWidget.h"
#include "UI/Shelter/JobSelectWidget.h"
#include "UI/Shelter/ShelterHUDWidget.h"

void AShelterHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();

	if (!HUDWidgetClass)
	{
		// 화면만 봐서는 "아직 안 만들었나" 와 구별되지 않는다
		UE_LOG(LogHeavyUI, Warning, TEXT("[ShelterHUD] HUDWidgetClass 가 비어 있다"));
	}
	else if (PC)
	{
		HUDWidget = CreateWidget<UShelterHUDWidget>(PC, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(HUDZOrder);

			if (UChatBoxWidget* Chat = HUDWidget->GetChatBox())
			{
				ChatDismissedHandle = Chat->OnChatDismissed.AddUObject(this, &AShelterHUD::HandleChatDismissed);
			}
		}
	}

	TryBindPlayerState();
}

void AShelterHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
	}

	if (AShelterPlayerState* PS = BoundState.Get())
	{
		PS->OnJobConfirmedChanged.RemoveDynamic(this, &AShelterHUD::HandleJobConfirmedChanged);
	}
	BoundState = nullptr;

	// 순서가 중요하다 — HUDWidget 을 먼저 비우면 채팅 구독을 뗄 대상이 사라진다
	if (HUDWidget && ChatDismissedHandle.IsValid())
	{
		if (UChatBoxWidget* Chat = HUDWidget->GetChatBox())
		{
			Chat->OnChatDismissed.Remove(ChatDismissedHandle);
		}
		ChatDismissedHandle.Reset();
	}

	if (JobSelectWidget)
	{
		JobSelectWidget->RemoveFromParent();
		JobSelectWidget = nullptr;
	}

	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AShelterHUD::TryBindPlayerState()
{
	if (BoundState)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayerController();
	AShelterPlayerState* PS = PC ? PC->GetPlayerState<AShelterPlayerState>() : nullptr;

	if (!PS)
	{
		// PlayerState 는 PlayerController 보다 늦게 온다. 접속 직후 nullptr 이 정상이다
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(BindRetryHandle, this,
				&AShelterHUD::TryBindPlayerState, BindRetryInterval, false);
		}
		return;
	}

	BoundState = PS;
	PS->OnJobConfirmedChanged.AddDynamic(this, &AShelterHUD::HandleJobConfirmedChanged);

	ApplyJobConfirmed(PS->IsJobConfirmed());
}

void AShelterHUD::HandleJobConfirmedChanged(AShelterPlayerState* PlayerState)
{
	if (PlayerState != BoundState)
	{
		return;   // 남의 PS 는 내 화면과 무관하다
	}

	ApplyJobConfirmed(PlayerState->IsJobConfirmed());
}

void AShelterHUD::ApplyJobConfirmed(bool bConfirmed)
{
	if (bConfirmed)
	{
		HideJobSelect();
	}
	else
	{
		ShowJobSelect();
	}
}

void AShelterHUD::ShowJobSelect()
{
	if (JobSelectWidget)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !JobSelectWidgetClass)
	{
		return;
	}

	JobSelectWidget = CreateWidget<UJobSelectWidget>(PC, JobSelectWidgetClass);
	if (!JobSelectWidget)
	{
		return;
	}

	JobSelectWidget->AddToViewport(JobSelectZOrder);

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(JobSelectWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
}

void AShelterHUD::HideJobSelect()
{
	// 이미 닫혀 있으면 입력 모드를 건드리지 않는다 —
	// 건드리면 채팅을 치고 있던 상태를 덮어 글자가 안 들어간다
	if (!JobSelectWidget)
	{
		return;
	}

	JobSelectWidget->RemoveFromParent();
	JobSelectWidget = nullptr;

	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void AShelterHUD::SetChatFocused(bool bFocused)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !HUDWidget)
	{
		return;
	}

	// 역할 선택 모달이 입력을 쥐고 있는 동안에는 채팅을 열지 않는다
	if (bFocused && JobSelectWidget)
	{
		return;
	}

	// 입력 모드를 바꾸면 입력창이 포커스를 잃으면서 OnTextCommitted 가 한 번 더 오고,
	// 그것이 다시 여기로 돌아온다. 이 가드가 없으면 그 자리에서 무한히 되돈다
	if (bChatFocused == bFocused)
	{
		return;
	}

	UChatBoxWidget* Chat = HUDWidget->GetChatBox();
	if (!Chat)
	{
		return;
	}

	bChatFocused = bFocused;

	if (bFocused)
	{
		// 순서가 중요하다 — 입력 모드를 먼저 UI 로 바꾸지 않으면
		// 게임 입력이 키를 먼저 먹어서 입력창에 글자가 들어가지 않는다
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);

		Chat->FocusInput();
	}
	else
	{
		Chat->ClearInput();
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void AShelterHUD::HandleChatDismissed()
{
	SetChatFocused(false);
}
