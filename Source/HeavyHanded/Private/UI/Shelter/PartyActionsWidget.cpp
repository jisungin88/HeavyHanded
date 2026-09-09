#include "UI/Shelter/PartyActionsWidget.h"

#include "Components/Button.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Core/PlayerControllers/ShelterPlayerController.h"

void UPartyActionsWidget::TryBind()
{
	if (BoundState)
	{
		return;
	}

	AShelterGameState* GS = GetShelterGameState();
	if (!GS)
	{
		ScheduleRebind();
		return;
	}

	BoundState = GS;
	GS->OnCanStartChanged.AddDynamic(this, &UPartyActionsWidget::HandleCanStartChanged);

	if (Btn_Start)
	{
		Btn_Start->OnClicked.AddDynamic(this, &UPartyActionsWidget::HandleStartClicked);

		const APlayerController* PC = GetOwningPlayer();
		const bool bHost = PC && PC->HasAuthority();
		Btn_Start->SetVisibility(bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Btn_Leave)
	{
		Btn_Leave->OnClicked.AddDynamic(this, &UPartyActionsWidget::HandleLeaveClicked);
	}

	ApplyCanStart(GS->bCanStart);
}

void UPartyActionsWidget::Unbind()
{
	if (AShelterGameState* GS = BoundState.Get())
	{
		GS->OnCanStartChanged.RemoveDynamic(this, &UPartyActionsWidget::HandleCanStartChanged);
	}

	if (Btn_Start)
	{
		Btn_Start->OnClicked.RemoveDynamic(this, &UPartyActionsWidget::HandleStartClicked);
	}

	if (Btn_Leave)
	{
		Btn_Leave->OnClicked.RemoveDynamic(this, &UPartyActionsWidget::HandleLeaveClicked);
	}

	BoundState = nullptr;
}

void UPartyActionsWidget::HandleCanStartChanged(bool bCanStart)
{
	ApplyCanStart(bCanStart);
}

void UPartyActionsWidget::ApplyCanStart(bool bCanStart)
{
	if (Btn_Start)
	{
		Btn_Start->SetIsEnabled(bCanStart);
	}

	OnCanStartUpdate(bCanStart);
}

void UPartyActionsWidget::HandleStartClicked()
{
	if (AShelterPlayerController* PC = GetShelterPC())
	{
		PC->IngameTravel();
	}
}

void UPartyActionsWidget::HandleLeaveClicked()
{
	if (AShelterPlayerController* PC = GetShelterPC())
	{
		PC->LeaveRoom(LeaveMapName);
	}
}
