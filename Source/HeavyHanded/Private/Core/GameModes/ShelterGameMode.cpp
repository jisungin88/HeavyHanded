// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameModes/ShelterGameMode.h"
#include "Core/GameStates/ShelterGameState.h"

void AShelterGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);


    if (AShelterGameState* GS = GetGameState<AShelterGameState>())
    {
        GS->UpdateLobbyPlayerCount();
    }

    //GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, 
      //  FString::Printf(TEXT("PlayerCount = %d"), GetGameState<AShelterGameState>()->GetLobbyPlayerCount()));

}

void AShelterGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    if (AShelterGameState* GS = GetGameState<AShelterGameState>())
    {
        GS->UpdateLobbyPlayerCount();
    }
}
