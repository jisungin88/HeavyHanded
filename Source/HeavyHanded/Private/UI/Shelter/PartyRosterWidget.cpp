#include "UI/Shelter/PartyRosterWidget.h"

#include "Core/GameStates/ShelterGameState.h"

void UPartyRosterWidget::TryBind()
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

	GS->OnJobStateChanged.AddDynamic(this, &UPartyRosterWidget::HandleJobStateChanged);
	GS->OnLobbyPlayerCountChanged.AddDynamic(this, &UPartyRosterWidget::HandlePlayerCountChanged);

	RefreshRoster();
}

void UPartyRosterWidget::Unbind()
{
	if (AShelterGameState* GS = BoundState.Get())
	{
		GS->OnJobStateChanged.RemoveDynamic(this, &UPartyRosterWidget::HandleJobStateChanged);
		GS->OnLobbyPlayerCountChanged.RemoveDynamic(this, &UPartyRosterWidget::HandlePlayerCountChanged);
	}
	BoundState = nullptr;
}

void UPartyRosterWidget::HandleJobStateChanged()
{
	RefreshRoster();
}

void UPartyRosterWidget::HandlePlayerCountChanged(int32 PlayerCount)
{
	RefreshRoster();
}

void UPartyRosterWidget::RefreshRoster()
{
	AShelterGameState* GS = BoundState.Get();
	if (!GS)
	{
		return;
	}

	OnRosterRefreshed(GS->GetShelterPlayerStates());
}
