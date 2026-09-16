#include "UI/Shelter/ChatBoxWidget.h"

#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Core/PlayerControllers/ShelterPlayerController.h"

void UChatBoxWidget::TryBind()
{
	if (BoundPC)
	{
		return;
	}

	AShelterPlayerController* PC = GetShelterPC();
	if (!PC)
	{
		ScheduleRebind();
		return;
	}

	BoundPC = PC;
	PC->OnChatMessageReceived.AddDynamic(this, &UChatBoxWidget::HandleChatMessage);

	if (Input_Chat)
	{
		Input_Chat->OnTextCommitted.AddDynamic(this, &UChatBoxWidget::HandleTextCommitted);
	}
}

void UChatBoxWidget::Unbind()
{
	if (AShelterPlayerController* PC = BoundPC.Get())
	{
		PC->OnChatMessageReceived.RemoveDynamic(this, &UChatBoxWidget::HandleChatMessage);
	}

	if (Input_Chat)
	{
		Input_Chat->OnTextCommitted.RemoveDynamic(this, &UChatBoxWidget::HandleTextCommitted);
	}

	BoundPC = nullptr;
}

void UChatBoxWidget::FocusInput()
{
	if (!Input_Chat)
	{
		return;
	}

	SetInputVisible(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (Input_Chat)
				{
					Input_Chat->SetKeyboardFocus();
				}
			}));
	}
}

void UChatBoxWidget::ClearInput()
{
	if (Input_Chat)
	{
		Input_Chat->SetText(FText::GetEmpty());
	}

	SetInputVisible(false);
}

void UChatBoxWidget::SetInputVisible(bool bVisible)
{
	if (!Input_Chat)
	{
		return;
	}

	Input_Chat->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UChatBoxWidget::HandleChatMessage(const FString& PlayerName, const FString& Message)
{
	OnChatLineAdded(FText::FromString(FString::Printf(TEXT("%s: %s"), *PlayerName, *Message)));

	while (Scroll_Chat && Scroll_Chat->GetChildrenCount() > MaxChatLine)
	{
		Scroll_Chat->RemoveChildAt(0);
	}
}

void UChatBoxWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter)
	{
		OnChatDismissed.Broadcast();
		return;
	}

	const FString Message = Text.ToString();
	ClearInput();

	if (AShelterPlayerController* PC = BoundPC.Get())
	{
		if (!Message.TrimStartAndEnd().IsEmpty())
		{
			PC->Server_SendChatMessage(Message);
		}
	}

	OnChatDismissed.Broadcast();
}
