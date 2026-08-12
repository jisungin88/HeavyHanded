// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerControllers/TitlePlayerController.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/Engine.h"


void ATitlePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
    {
        SessionInterface = OSS->GetSessionInterface();
    }


    // PlayerController 시작 시 현재 세션 상태 확인
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
        TEXT("===== TitlePlayerController BeginPlay ====="));
    UE_LOG(LogTemp, Warning, TEXT("===== TitlePlayerController BeginPlay ====="));

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
        SessionInterface.IsValid()
        ? TEXT("BeginPlay: SessionInterface OK")
        : TEXT("BeginPlay: SessionInterface INVALID"));
    UE_LOG(LogTemp, Warning,
        TEXT("BeginPlay: SessionInterface = %s"),
        SessionInterface.IsValid() ? TEXT("OK") : TEXT("INVALID"));

    const bool bHasSession =
        SessionInterface.IsValid() &&
        SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;

    GEngine->AddOnScreenDebugMessage(-1, 5.0f,
        bHasSession ? FColor::Red : FColor::Green,
        bHasSession
        ? TEXT("BeginPlay: GameSession EXISTS")
        : TEXT("BeginPlay: GameSession NOT FOUND"));

    UE_LOG(LogTemp, Warning,
        TEXT("BeginPlay: GameSession = %s"),
        bHasSession ? TEXT("EXISTS") : TEXT("NOT FOUND"));


}


// 생성
void ATitlePlayerController::TitleCreateSession
    (const FString& RoomName, int maxPlayer, bool isPublic)
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();


    // CreateSession 호출 여부 확인
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
        TEXT("===== TitleCreateSession CALLED ====="));
    UE_LOG(LogTemp, Warning, TEXT("===== TitleCreateSession CALLED ====="));


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



// 검색
void ATitlePlayerController::TitleFindSessions()
{

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("===== FindSessions Debug Start ====="));


    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, IsLocalController() ? 
        TEXT("IsLocalController = TRUE") : TEXT("IsLocalController = FALSE"));

    FString NetModeString;
    switch (GetNetMode())
    {
    case NM_Standalone: NetModeString = TEXT("Standalone"); break;
    case NM_DedicatedServer: NetModeString = TEXT("DedicatedServer"); break;
    case NM_ListenServer: NetModeString = TEXT("ListenServer"); break;
    case NM_Client: NetModeString = TEXT("Client"); break;
    default: NetModeString = TEXT("Unknown"); break;
    }

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, 
        FString::Printf(TEXT("NetMode = %s"), *NetModeString));

    ULocalPlayer* LP = GetLocalPlayer();

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, LP ? 
        TEXT("LocalPlayer = VALID") : TEXT("LocalPlayer = INVALID"));

    if (LP)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, 
            FString::Printf(TEXT("ControllerId = %d"), LP->GetControllerId()));
    }

    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, OSS ? 
        *FString::Printf(TEXT("OSS = %s"), *OSS->GetSubsystemName().ToString()) : TEXT("OSS = NULL"));

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, SessionInterface.IsValid() ? 
        
        TEXT("SessionInterface = VALID") : TEXT("SessionInterface = INVALID"));



    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("PlayerController = %s"), *GetName()));



    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("===== FindSessions Debug Start ====="));





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
    PublicSessionResults.Reset();

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
        PublicSessionResults.Add(Result);

    } //for



    // 검색 완료 후 UI 갱신 알림
    OnRoomListUpdated.Broadcast(bWasSuccessful);

}



// 참가
void ATitlePlayerController::TitleJoinSession(int32 SearchIndex)
{
    // 세션 참가 시작
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
        TEXT("===== Join Session Start ====="));
    UE_LOG(LogTemp, Warning, TEXT("===== Join Session Start ====="));

    // SessionInterface 유효성 확인
    if (!SessionInterface.IsValid())
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
            TEXT("Join: SessionInterface INVALID"));
        UE_LOG(LogTemp, Warning, TEXT("Join: SessionInterface INVALID"));
        return;
    }

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
        TEXT("Join: SessionInterface OK"));
    UE_LOG(LogTemp, Warning, TEXT("Join: SessionInterface OK"));

    // 검색 결과 인덱스 확인
    if (!PublicSessionResults.IsValidIndex(SearchIndex))
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
            FString::Printf(TEXT("Join: Invalid Index = %d / Num = %d"), SearchIndex, PublicSessionResults.Num()));
        UE_LOG(LogTemp, Warning, TEXT("Join: Invalid Index = %d / Num = %d"), SearchIndex, PublicSessionResults.Num());
        return;
    }

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
        FString::Printf(TEXT("Join: Valid Index = %d / Num = %d"), SearchIndex, PublicSessionResults.Num()));
    UE_LOG(LogTemp, Warning, TEXT("Join: Valid Index = %d / Num = %d"), SearchIndex, PublicSessionResults.Num());

    // 현재 GameSession이 이미 존재하는지 확인
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
        SessionInterface->GetNamedSession(NAME_GameSession)
        ? TEXT("Join: Existing GameSession")
        : TEXT("Join: No Existing GameSession"));

    UE_LOG(LogTemp, Warning,
        TEXT("Join: Existing GameSession = %s"),
        SessionInterface->GetNamedSession(NAME_GameSession) ? TEXT("YES") : TEXT("NO"));

    // Join 완료 델리게이트 등록
    JoinHandle =
        SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
            FOnJoinSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnJoinSessionComplete));

    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
        TEXT("Join: Delegate Added"));
    UE_LOG(LogTemp, Warning, TEXT("Join: Delegate Added"));

    // 선택한 세션 검색 결과 가져오기
    const FOnlineSessionSearchResult& Result =
        PublicSessionResults[SearchIndex];

    // 세션 ID 확인
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
        FString::Printf(TEXT("Join: Session ID = %s"), *Result.GetSessionIdStr()));
    UE_LOG(LogTemp, Warning, TEXT("Join: Session ID = %s"), *Result.GetSessionIdStr());

    // 선택한 세션에 참가 요청
    bool bStarted =
        SessionInterface->JoinSession(
            0,
            NAME_GameSession,
            Result);

    // JoinSession 호출 자체가 성공했는지 확인
    if (bStarted)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            TEXT("JoinSession SUCCESS"));
        UE_LOG(LogTemp, Warning, TEXT("JoinSession SUCCESS"));
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
            TEXT("JoinSession FAILED"));
        UE_LOG(LogTemp, Error, TEXT("JoinSession FAILED"));
    }

    // 세션 참가 요청 종료
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan,
        TEXT("===== Join Session End ====="));
    UE_LOG(LogTemp, Warning, TEXT("===== Join Session End ====="));
    
    
}

void ATitlePlayerController::TitleOnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
            JoinHandle);
    }


    UE_LOG(LogTemp, Warning,
        TEXT("===== Join Complete ====="));

    UE_LOG(LogTemp, Warning,
        TEXT("Join Result = %d"),
        static_cast<int32>(Result));


    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Error,
            TEXT("JOIN FAILED"));
        return;
    }


    FString ConnectString;

    bool bResolved =
        SessionInterface->GetResolvedConnectString(
            SessionName,
            ConnectString);

    UE_LOG(LogTemp, Warning,
        TEXT("Resolve Connect String = %d"),
        bResolved);

    UE_LOG(LogTemp, Warning,
        TEXT("ConnectString = %s"),
        *ConnectString);


    if (!bResolved || ConnectString.IsEmpty())
    {
        return;
    }


    ClientTravel(
        ConnectString,
        TRAVEL_Absolute);


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
