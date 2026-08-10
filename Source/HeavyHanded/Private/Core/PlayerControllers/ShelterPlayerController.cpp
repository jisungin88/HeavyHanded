// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerControllers/ShelterPlayerController.h"
#include "Core/GameStates/ShelterGameState.h"
#include "GameFramework/PlayerState.h"

void AShelterPlayerController::BeginPlay()
{
    Super::BeginPlay();


    //if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
    //{
    //    SessionInterface = OSS->GetSessionInterface();
    //}
    //
    //if (SessionInterface.IsValid())
    //{
    //    FNamedOnlineSession* Session =
    //        SessionInterface->GetNamedSession(NAME_GameSession);
    //
    //    if (Session)
    //    {
    //        UE_LOG(LogTemp, Warning,
    //            TEXT("After Travel Session Exists"));
    //
    //        UE_LOG(LogTemp, Warning,
    //            TEXT("State = %d"),
    //            (int32)Session->SessionState);
    //
    //        UE_LOG(LogTemp, Warning,
    //            TEXT("Advertise = %d"),
    //            Session->SessionSettings.bShouldAdvertise);
    //
    //        UE_LOG(LogTemp, Warning,
    //            TEXT("LAN = %d"),
    //            Session->SessionSettings.bIsLANMatch);
    //    }
    //    else
    //    {
    //        UE_LOG(LogTemp, Warning,
    //            TEXT("After Travel Session Missing"));
    //    }
    //}




   //// 클라이언트는 BeginPlay 시점에 GameState가 아직 없을 수도 있음
   //if (AShelterGameState* GS = GetWorld()->GetGameState<AShelterGameState>())
   //{
   //   GS->OnPlayerListChanged.AddDynamic(
   //       this,
   //       &AShelterPlayerController::HandlePlayerListChanged);
   //   
   //   // 최초 1회 갱신
   //   HandlePlayerListChanged();
   //}

}

void AShelterPlayerController::HandlePlayerListChanged()
{
    BP_OnPlayerListChanged();
}


