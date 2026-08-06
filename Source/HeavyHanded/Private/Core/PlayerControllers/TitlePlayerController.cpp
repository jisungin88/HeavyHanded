// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerControllers/TitlePlayerController.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"


void ATitlePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
    {
        SessionInterface = OSS->GetSessionInterface();
    }
}

void ATitlePlayerController::TitleCreateSession
    (const FString& RoomName, int maxPlayer, bool isPublic)
{
    if (!SessionInterface.IsValid())
        return;

    if (SessionInterface->GetNamedSession(NAME_GameSession))
    {
        SessionInterface->DestroySession(NAME_GameSession);
    }

    CreateHandle =
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
            FOnCreateSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnCreateSessionComplete));

    FOnlineSessionSettings Settings;

    Settings.bIsLANMatch = true;
    Settings.NumPublicConnections = maxPlayer;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = false;
    Settings.bUsesPresence = true;


    Settings.Set(
        FName(TEXT("ROOM_NAME")),
        RoomName,
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    );

    // 비공개 방만 코드 생성
    if (!isPublic)
    {
        FString RoomCode = GenerateRoomCode();

        Settings.Set(
            FName(TEXT("ROOM_CODE")),
            RoomCode,
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );
    }

    SessionInterface->CreateSession(
        0,
        NAME_GameSession,
        Settings);

}

void ATitlePlayerController::TitleFindSessions()
{
    FindHandle =
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
            FOnFindSessionsCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnFindSessionsComplete
            )
        );
}

void ATitlePlayerController::TitleJoinSession(int32 SearchIndex)
{
}


void ATitlePlayerController::TitleOnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{

    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
            CreateHandle);
    }


    if (bWasSuccessful)
    {
        UE_LOG(LogTemp, Log, TEXT("Session Created"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Session Create Failed"));
    }

    OnSessionCreated.Broadcast(bWasSuccessful);
}


void ATitlePlayerController::TitleOnFindSessionsComplete(bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
            FindHandle
        );
    }


    if (!bWasSuccessful || !SessionSearch.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Find Session Failed"));
        return;
    }


    UE_LOG(LogTemp, Log,
        TEXT("Found Sessions : %d"),
        SessionSearch->SearchResults.Num());


    for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
    {
        FString RoomName;
        FString RoomCode;


        Result.Session.SessionSettings.Get(
            FName(TEXT("ROOM_NAME")),
            RoomName
        );


        bool bHasRoomCode =
            Result.Session.SessionSettings.Get(
                FName(TEXT("ROOM_CODE")),
                RoomCode
            );


        // 비공개 방 제외
        if (bHasRoomCode)
        {
            continue;
        }


        UE_LOG(LogTemp, Log,
            TEXT("Public Room : %s"),
            *RoomName);
    
    
        FRoomListData NewRoom;

        NewRoom.RoomName = RoomName;

        NewRoom.CurrentPlayers =
            Result.Session.SessionSettings.NumPublicConnections -
            Result.Session.NumOpenPublicConnections;

        NewRoom.MaxPlayers =
            Result.Session.SessionSettings.NumPublicConnections;


        RoomList.Add(NewRoom);


    } //for



    // 검색 완료 후 UI 갱신 알림
    OnRoomListUpdated.Broadcast();

}

void ATitlePlayerController::TitleOnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
}

FString ATitlePlayerController::GenerateRoomCode()
{
    const FString Characters = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    FString Code;

    for (int32 i = 0; i < 6; i++)
    {
        Code += Characters[FMath::RandRange(0, Characters.Len() - 1)];
    }

    return Code;
}
