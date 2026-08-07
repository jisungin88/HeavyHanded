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
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();

    if (OSS)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("OSS = %s"),
            *OSS->GetSubsystemName().ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("OSS NULL"));
    }


    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("SessionInterface Not Valid"));
        return;
    }


    // if (SessionInterface->GetNamedSession(NAME_GameSession))
    // {
    //     return;
    //     //UE_LOG(LogTemp, Warning, TEXT("Existing Session Found"));
    //     //SessionInterface->DestroySession(NAME_GameSession);
    // }





    CreateHandle =
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
            FOnCreateSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnCreateSessionComplete));

    UE_LOG(LogTemp, Warning, TEXT("Create Delegate Added"));



    FOnlineSessionSettings Settings;

    Settings.bIsLANMatch = true;
    Settings.NumPublicConnections = maxPlayer;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bUsesPresence = false;


    
    Settings.Set(
        FName(TEXT("ROOM_NAME")),
        RoomName,
        //EOnlineDataAdvertisementType::ViaPing
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
    


    StartHandle =
        SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
            FOnStartSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnStartSessionComplete));



    bool bCreateStarted = SessionInterface->CreateSession(
        0,
        NAME_GameSession,
        Settings);

    UE_LOG(LogTemp, Warning,
        TEXT("CreateSession Called = %d"),
        bCreateStarted);

}

void ATitlePlayerController::TitleFindSessions()
{

    UE_LOG(LogTemp, Warning, TEXT("===== FindSessions Debug Start ====="));

    // 현재 컨트롤러가 로컬 플레이어인지
    UE_LOG(LogTemp, Warning,
        TEXT("IsLocalController = %d"),
        IsLocalController());

    // 현재 NetMode 확인
    FString NetModeString;

    switch (GetNetMode())
    {
    case NM_Standalone:
        NetModeString = TEXT("Standalone");
        break;

    case NM_DedicatedServer:
        NetModeString = TEXT("DedicatedServer");
        break;

    case NM_ListenServer:
        NetModeString = TEXT("ListenServer");
        break;

    case NM_Client:
        NetModeString = TEXT("Client");
        break;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("NetMode = %s"),
        *NetModeString);


    // LocalPlayer 확인
    ULocalPlayer* LP = GetLocalPlayer();

    UE_LOG(LogTemp, Warning,
        TEXT("LocalPlayer Valid = %d"),
        LP != nullptr);


    if (LP)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ControllerId = %d"),
            LP->GetControllerId());
    }


    // OnlineSubsystem 확인
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();

    UE_LOG(LogTemp, Warning,
        TEXT("OSS = %s"),
        OSS ? *OSS->GetSubsystemName().ToString() : TEXT("NULL"));


    // SessionInterface 확인
    UE_LOG(LogTemp, Warning,
        TEXT("SessionInterface Valid = %d"),
        SessionInterface.IsValid());





    // -----------------------------

    
    //FString NetModeString;

    // switch (GetNetMode())
    // {
    // case NM_Standalone:
    //     NetModeString = TEXT("Standalone");
    //     break;
    // 
    // case NM_DedicatedServer:
    //     NetModeString = TEXT("DedicatedServer");
    //     break;
    // 
    // case NM_ListenServer:
    //     NetModeString = TEXT("ListenServer");
    //     break;
    // 
    // case NM_Client:
    //     NetModeString = TEXT("Client");
    //     break;
    // 
    // default:
    //     NetModeString = TEXT("Unknown");
    //     break;
    // }

    //UE_LOG(LogTemp, Warning,
    //    TEXT("NetMode = %s"),
    //    *NetModeString);

    UE_LOG(LogTemp, Warning,
        TEXT("IsLocalController = %d"),
        IsLocalController());

    UE_LOG(LogTemp, Warning,
        TEXT("PlayerController Name = %s"),
        *GetName());

        

    UE_LOG(LogTemp, Warning, TEXT("===== FindSessions Debug End ====="));


    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("SessionInterface Not Valid"));
        return;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = true;
    SessionSearch->MaxSearchResults = 10;


    FindHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(
            this,
            &ATitlePlayerController::TitleOnFindSessionsComplete
        )
    );

    //bool bStarted = SessionInterface->FindSessions(
    //    0,
    //    SessionSearch.ToSharedRef()
    //);


    int32 LocalPlayerNum = 0;
    
    if (GetLocalPlayer())
    {
        LocalPlayerNum = GetLocalPlayer()->GetControllerId();
    }
    
    bool bStarted = SessionInterface->FindSessions(
        LocalPlayerNum,
        SessionSearch.ToSharedRef()
    );


    UE_LOG(LogTemp, Warning,
        TEXT("FindSessions Started = %d"),
        bStarted);


}

void ATitlePlayerController::TitleJoinSession(int32 SearchIndex)
//void ATitlePlayerController::TitleJoinSession(int32 SearchIndex, const FOnlineSessionSearchResult& Result)
{
    /*
    if (!SessionInterface.IsValid())
    {
        return;
    }


    JoinHandle =
        SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
            FOnJoinSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnJoinSessionComplete)
        );


    bool bStarted =
        SessionInterface->JoinSession(
            0,
            NAME_GameSession,
            Result
        );


    UE_LOG(LogTemp, Warning,
        TEXT("JoinSession Started = %d"),
        bStarted);
        */
}


void ATitlePlayerController::TitleOnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{

    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
            CreateHandle);
    }


    if (!bWasSuccessful)
    {
        UE_LOG(LogTemp, Error, TEXT("Session Create Failed"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Session Created"));



    FNamedOnlineSession* Session =
        SessionInterface->GetNamedSession(NAME_GameSession);

    UE_LOG(LogTemp, Warning,
        TEXT("Session Exists = %s"),
        Session ? TEXT("YES") : TEXT("NO"));

    if (Session)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Session State = %d"),
            (int32)Session->SessionState);

        UE_LOG(LogTemp, Warning,
            TEXT("Advertise = %d"),
            Session->SessionSettings.bShouldAdvertise);

        UE_LOG(LogTemp, Warning,
            TEXT("LAN = %d"),
            Session->SessionSettings.bIsLANMatch);

        UE_LOG(LogTemp, Warning,
            TEXT("Connections = %d"),
            Session->SessionSettings.NumPublicConnections);

        UE_LOG(LogTemp, Warning,
            TEXT("HOST SESSION OK"));

        UE_LOG(LogTemp, Warning,
            TEXT("Advertise=%d LAN=%d"),
            Session->SessionSettings.bShouldAdvertise,
            Session->SessionSettings.bIsLANMatch);
    }

    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("HOST SESSION NONE"));
    }


    //SessionInterface->StartSession(NAME_GameSession);











    OnSessionCreated.Broadcast(bWasSuccessful);

}

void ATitlePlayerController::TitleOnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Warning,
        TEXT("StartSession Success = %d"),
        bWasSuccessful);

    FNamedOnlineSession* Session =
        SessionInterface->GetNamedSession(NAME_GameSession);

    if (Session)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("State After StartSession = %d"),
            (int32)Session->SessionState);
    }


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


    UE_LOG(LogTemp, Warning, TEXT("Find Success = %d"), bWasSuccessful);
    UE_LOG(LogTemp, Warning, TEXT("Results = %d"), SessionSearch->SearchResults.Num());

    //RoomList.Empty();
    RoomList.Reset();


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

        NewRoom.SessionResult = Result;

        RoomList.Add(NewRoom);


    } //for



    // 검색 완료 후 UI 갱신 알림
    OnRoomListUpdated.Broadcast(bWasSuccessful);

}

void ATitlePlayerController::TitleOnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
            JoinHandle);
    }


    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Join Failed : %d"),
            (int32)Result);

        return;
    }


    UE_LOG(LogTemp, Warning,
        TEXT("Join Success"));


    FString ConnectString;


    if (!SessionInterface->GetResolvedConnectString(
        SessionName,
        ConnectString))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Get Connect String Failed"));
        return;
    }


    UE_LOG(LogTemp, Warning,
        TEXT("Connect String : %s"),
        *ConnectString);


    ClientTravel(
        ConnectString,
        TRAVEL_Absolute
    );


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
