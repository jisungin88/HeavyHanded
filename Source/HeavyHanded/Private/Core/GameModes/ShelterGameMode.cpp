// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameModes/ShelterGameMode.h"
#include "Core/GameStates/ShelterGameState.h"

void AShelterGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (AShelterGameState* GS = GetGameState<AShelterGameState>())
    {
        GS->BroadcastPlayerListChanged();
    }
}

void AShelterGameMode::Logout(AController* Exiting)
{
    if (AShelterGameState* GS = GetGameState<AShelterGameState>())
    {
        GS->BroadcastPlayerListChanged();
    }
}
