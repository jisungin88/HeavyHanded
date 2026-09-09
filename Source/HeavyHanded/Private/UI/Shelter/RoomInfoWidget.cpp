#include "UI/Shelter/RoomInfoWidget.h"

#include "Components/TextBlock.h"
#include "Core/GameInstances/NetGameInstanceSubsystem.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Engine/GameInstance.h"

void URoomInfoWidget::TryBind()
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
	GS->OnLobbyPlayerCountChanged.AddDynamic(this, &URoomInfoWidget::HandlePlayerCountChanged);

	ApplyRoomText();
	ApplyPlayerCount(GS->GetLobbyPlayerCount());
}

void URoomInfoWidget::Unbind()
{
	if (AShelterGameState* GS = BoundState.Get())
	{
		GS->OnLobbyPlayerCountChanged.RemoveDynamic(this, &URoomInfoWidget::HandlePlayerCountChanged);
	}
	BoundState = nullptr;
}

void URoomInfoWidget::HandlePlayerCountChanged(int32 PlayerCount)
{
	ApplyPlayerCount(PlayerCount);
}

void URoomInfoWidget::ApplyRoomText()
{
	const UGameInstance* GI = GetGameInstance();
	const UNetGameInstanceSubsystem* Net = GI ? GI->GetSubsystem<UNetGameInstanceSubsystem>() : nullptr;
	if (!Net)
	{
		return;
	}

	if (Txt_RoomName)
	{
		Txt_RoomName->SetText(FText::FromString(Net->GetJoinedRoomName()));
	}

	if (Txt_RoomCode)
	{
		Txt_RoomCode->SetText(FText::FromString(Net->GetJoinedRoomCode()));
	}
}

void URoomInfoWidget::ApplyPlayerCount(int32 PlayerCount)
{
	if (!Txt_PlayerCount)
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	const UNetGameInstanceSubsystem* Net = GI ? GI->GetSubsystem<UNetGameInstanceSubsystem>() : nullptr;
	const int32 MaxPlayers = Net ? Net->GetRoomMaxPlayer() : 0;

	Txt_PlayerCount->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), PlayerCount, MaxPlayers)));
}
