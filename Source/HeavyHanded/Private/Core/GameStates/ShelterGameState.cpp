// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameStates/ShelterGameState.h"

int32 AShelterGameState::GetLobbyPlayerCount() const
{
	return PlayerArray.Num();
}

void AShelterGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	UpdateLobbyPlayerCount();
}

void AShelterGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	UpdateLobbyPlayerCount();
}

void AShelterGameState::UpdateLobbyPlayerCount()
{
	const int32 PlayerCount = PlayerArray.Num();

	OnLobbyPlayerCountChanged.Broadcast(PlayerCount);
}
