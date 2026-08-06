// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerControllers/ShelterPlayerController.h"
#include "Core/GameStates/ShelterGameState.h"
#include "GameFramework/PlayerState.h"

void AShelterPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 클라이언트는 BeginPlay 시점에 GameState가 아직 없을 수도 있음
    if (AShelterGameState* GS = GetWorld()->GetGameState<AShelterGameState>())
    {
       GS->OnPlayerListChanged.AddDynamic(
           this,
           &AShelterPlayerController::HandlePlayerListChanged);
       
       // 최초 1회 갱신
       HandlePlayerListChanged();
    }

}

void AShelterPlayerController::HandlePlayerListChanged()
{
    BP_OnPlayerListChanged();
}


