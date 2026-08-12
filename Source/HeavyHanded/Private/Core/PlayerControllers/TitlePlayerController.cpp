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
		SessionDebug(TEXT("OSS"), OSS != nullptr);
	}



	SessionDebug("SessionInterface", SessionInterface.IsValid());
	if (!SessionInterface.IsValid()) { return; }


	const bool bCanCreateSession =
		SessionInterface.IsValid() &&
		SessionInterface->GetNamedSession(NAME_GameSession) == nullptr;



	SessionDebug("BeginPlay: bCanCreateSession", bCanCreateSession);


}

void ATitlePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SessionInterface.IsValid())
	{
		FNamedOnlineSession* Session =
			SessionInterface->GetNamedSession(NAME_GameSession);

		if (Session)
		{
			UE_LOG(LogTemp, Warning, TEXT("EndPlay: Destroying GameSession"));

			SessionInterface->DestroySession(NAME_GameSession);
		}
	}

	Super::EndPlay(EndPlayReason);
}


// 생성
void ATitlePlayerController::TitleCreateSession
    (const FString& RoomName, int maxPlayer, bool isPublic)
{

	// CreateSession 호출 여부 확인
	SessionDebug(TEXT("TitleCreateSession ================="), true);


	





	// 기존 GameSession이 있는지 확인
	FNamedOnlineSession* ExistingSession =
		SessionInterface->GetNamedSession(NAME_GameSession);

	SessionDebug(TEXT("Existing GameSession"),ExistingSession == nullptr);



    CreateHandle =
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
            FOnCreateSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnCreateSessionComplete));

	SessionDebug("Create Delegate Added", CreateHandle.IsValid());
	if (!CreateHandle.IsValid()) { return; }


	// GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Create Delegate = ADDED"));
	// UE_LOG(LogTemp, Warning, TEXT("Create Delegate Added"));
	// OnSessionDebug.Broadcast(TEXT("Create Delegate = ADDED"), true);



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
    // if (!isPublic)
    // {
    //     FString RoomCode = GenerateRoomCode();
	// 
    //     Settings.Set(
    //         FName(TEXT("ROOM_CODE")),
    //         RoomCode,
    //         EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    //     );
    // }
    


    StartHandle =
        SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
            FOnStartSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnStartSessionComplete));


	SessionDebug("Start Delegate Added", StartHandle.IsValid());
	if (!StartHandle.IsValid()) { return; }



	// 기존 세션이 있으면 삭제 후 다시 생성
	if (ExistingSession)
	{
		DestroyHandle =
			SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
				FOnDestroySessionCompleteDelegate::CreateUObject(
					this,
					&ATitlePlayerController::TitleOnDestroySessionComplete));

		SessionDebug(TEXT("Destroy Delegate Added"), DestroyHandle.IsValid());

		if (!DestroyHandle.IsValid())
		{
			bPendingCreateSession = false;
			return;
		}

		bool bDestroyStarted =
			SessionInterface->DestroySession(NAME_GameSession);

		SessionDebug(TEXT("DestroySession"), DestroyHandle.IsValid());
		return;
	}


    bool bCreateStarted = SessionInterface->CreateSession(
        0,
        NAME_GameSession,
        Settings);

	SessionDebug("CreateSession", bCreateStarted);

}

void ATitlePlayerController::TitleOnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{

    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    }


	SessionDebug(TEXT("CreateSession Complete"), bWasSuccessful);
    if (!bWasSuccessful) { return; }



	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);

	SessionDebug(TEXT("Created Session Exists"), Session != nullptr);

	if (Session)
	{
		SessionDebug(TEXT("CS_LAN"), Session->SessionSettings.bIsLANMatch);
		SessionDebug(TEXT("CS_Advertise"), Session->SessionSettings.bShouldAdvertise);
		SessionDebug(TEXT("CS_Connections"), Session->SessionSettings.NumPublicConnections > 0);
	}



    //if (Session)
    //{
    //    UE_LOG(LogTemp, Warning,
    //        TEXT("Session State = %d"),
    //        (int32)Session->SessionState);
	//
    //    UE_LOG(LogTemp, Warning,
    //        TEXT("Advertise = %d"),
    //        Session->SessionSettings.bShouldAdvertise);
	//
    //    UE_LOG(LogTemp, Warning,
    //        TEXT("LAN = %d"),
    //        Session->SessionSettings.bIsLANMatch);
	//
    //    UE_LOG(LogTemp, Warning,
    //        TEXT("Connections = %d"),
    //        Session->SessionSettings.NumPublicConnections);
	//
    //    UE_LOG(LogTemp, Warning,
    //        TEXT("HOST SESSION OK"));
	//
    //    UE_LOG(LogTemp, Warning,
    //        TEXT("Advertise=%d LAN=%d"),
    //        Session->SessionSettings.bShouldAdvertise,
    //        Session->SessionSettings.bIsLANMatch);
    //}

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

	SessionDebug(TEXT("===== FindSessions CALLED ====="), true);

	FString NetModeString;
	switch (GetNetMode())
	{
	case NM_Standalone: NetModeString = TEXT("Standalone"); break;
	case NM_DedicatedServer: NetModeString = TEXT("DedicatedServer"); break;
	case NM_ListenServer: NetModeString = TEXT("ListenServer"); break;
	case NM_Client: NetModeString = TEXT("Client"); break;
	default: NetModeString = TEXT("Unknown"); break;
	}
	SessionDebug(FString::Printf(TEXT("NetMode = %s"), *NetModeString), true);


    ULocalPlayer* LP = GetLocalPlayer();
	SessionDebug(TEXT("LocalPlayer"), LP != nullptr);

    if (LP)
    {
		SessionDebug(FString::Printf(TEXT("ControllerId = %d"), LP->GetControllerId()), true);
    }

    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	SessionDebug(TEXT("OSS"), OSS != nullptr);



    //GEngine->//AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("PlayerController = %s"), *GetName()));






	SessionDebug("SessionInterface", SessionInterface.IsValid());
	if (!SessionInterface.IsValid()) { return; }


    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = true;
    SessionSearch->MaxSearchResults = 10;

	SessionDebug(TEXT("SessionSearch Created"), SessionSearch.IsValid());

    FindHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(
            this,
            &ATitlePlayerController::TitleOnFindSessionsComplete
        )
    );

	SessionDebug(TEXT("Find Delegate Added"), FindHandle.IsValid());

	if (!FindHandle.IsValid()){return;}

    
    int32 LocalPlayerNum = 0;
    
    if (GetLocalPlayer())
    {
        LocalPlayerNum = GetLocalPlayer()->GetControllerId();
    }
	SessionDebug(TEXT("LocalPlayer"), GetLocalPlayer() != nullptr);


    bool bStarted = SessionInterface->FindSessions(
        LocalPlayerNum,
        SessionSearch.ToSharedRef()
    );



	SessionDebug(TEXT("FindSessions"), bStarted);

	SessionDebug(TEXT("===== FindSessions CALLED ====="), true);


}

void ATitlePlayerController::TitleOnFindSessionsComplete(bool bWasSuccessful)
{
    if (SessionInterface.IsValid())
    {
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);

		FindHandle.Reset(); //?
		SessionDebug(TEXT("Find Delegate Cleared"), true);
    }



	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		SessionDebug(TEXT("Find Session Failed"), false);
		return;
	}

	SessionDebug(TEXT("FindSessions Complete"), bWasSuccessful);
	SessionDebug(TEXT("SessionSearch Valid"), SessionSearch.IsValid());



	SessionDebug(FString::Printf(TEXT("Search Results = %d"),
		SessionSearch->SearchResults.Num()),
		SessionSearch->SearchResults.Num()>0);

    //RoomList.Empty();
	RoomList.Reset();
	PublicSessionResults.Reset();



    for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
    {
        FString RoomName;
        FString RoomCode;


 		bool bHasRoomName = Result.Session.SessionSettings.Get(FName(TEXT("ROOM_NAME")), RoomName);
		SessionDebug(TEXT("ROOM_NAME Found"), bHasRoomName);

		//bool bHasRoomCode = Result.Session.SessionSettings.Get(FName(TEXT("ROOM_CODE")), RoomCode);
		//SessionDebug(TEXT("ROOM_CODE Found"), bHasRoomCode);



		//임시 제거
        ///// // 비공개 방 제외
        ///// if (bHasRoomCode)
        ///// {
        /////     continue;
        ///// }


		SessionDebug(FString::Printf(TEXT("Public Room Name = %s"), *RoomName), true);
    
    
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


		SessionDebug(FString::Printf(
			TEXT("Players: %d / %d, Open: %d"),
			Result.Session.SessionSettings.NumPublicConnections -
			Result.Session.NumOpenPublicConnections,
			Result.Session.SessionSettings.NumPublicConnections,
			Result.Session.NumOpenPublicConnections
		), true);


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

void ATitlePlayerController::SessionDebug(const FString& Message, bool bSuccess)
{
	const FColor DebugColor = bSuccess ? FColor::Green : FColor::Red;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, DebugColor, *Message);
	}

	OnSessionDebug.Broadcast(Message, bSuccess);

	//UE_LOG(LogTemp, Warning, TEXT("[SessionDebug] %s"), *Message);
}

void ATitlePlayerController::TitleOnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid() && DestroyHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			DestroyHandle);

		DestroyHandle.Reset();
	}

	SessionDebug(TEXT("DestroySession Complete?"),bWasSuccessful);

	if (!bWasSuccessful)
	{
		bPendingCreateSession = false;
		return;
	}

	if (bPendingCreateSession)
	{
		bPendingCreateSession = false;

		SessionDebug(TEXT("Creating New Session"), true);

		//TitleCreateSessionInternal(
		//	PendingRoomName,
		//	PendingMaxPlayer,
		//	PendingIsPublic);
	}

}
