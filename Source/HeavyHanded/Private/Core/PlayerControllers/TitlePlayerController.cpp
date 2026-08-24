// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerControllers/TitlePlayerController.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Engine/Engine.h"
#include "Core/GameInstances/NetGameInstanceSubsystem.h"


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

	// 삭제 후 재시도로 이 함수가 다시 불릴 수 있다.
	// 남은 핸들 위에 덧붙이면 완료 콜백이 두 번 불린다
	if (CreateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		CreateHandle.Reset();
	}

    CreateHandle =
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
            FOnCreateSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnCreateSessionComplete));

	SessionDebug("Create Delegate Added", CreateHandle.IsValid());
	if (!CreateHandle.IsValid()) { return; }


    FOnlineSessionSettings Settings;

    Settings.bIsLANMatch = true;
    Settings.NumPublicConnections = maxPlayer;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bUsesPresence = false;

	// NetGameInstanceSubsystem에 저장
	UNetGameInstanceSubsystem* NetSubsystem =
		GetGameInstance()->GetSubsystem<UNetGameInstanceSubsystem>();


	FString RoomNameRand = RoomName.IsEmpty() ? FString::Printf(TEXT("랜덤 방 제목_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)) : RoomName;

    Settings.Set(
        FName(TEXT("ROOM_NAME")),
		RoomNameRand,
        //EOnlineDataAdvertisementType::ViaPing
        EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
    );

	if (NetSubsystem)
	{
		NetSubsystem->JoinedRoomName = RoomNameRand;
		NetSubsystem->MaxPlayers = maxPlayer;
	}
    
    if (!isPublic)
    {
        FString RoomCode = GenerateRoomCode();
	
        Settings.Set(
            FName(TEXT("ROOM_CODE")),
            RoomCode,
            EOnlineDataAdvertisementType::ViaOnlineServiceAndPing
        );

		if (NetSubsystem)
		{
			NetSubsystem->JoinedRoomCode = RoomCode;
		}

    }


	





	if (StartHandle.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartHandle);
		StartHandle.Reset();
	}

    StartHandle =
        SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
            FOnStartSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnStartSessionComplete));


	SessionDebug("Start Delegate Added", StartHandle.IsValid());
	if (!StartHandle.IsValid()) { return; }



	// 기존 세션이 있으면 삭제 후 다시 생성한다.
	// 삭제는 비동기라 여기서 끝내고, 이어서 하는 것은 TitleOnDestroySessionComplete 가 맡는다
	if (ExistingSession)
	{
		PendingAction = EPendingSessionAction::Create;
		PendingRoomName = RoomName;
		PendingMaxPlayer = maxPlayer;
		bPendingIsPublic = isPublic;

		// 실패하면 안에서 예약을 되돌리고 로그를 남긴다. 어느 쪽이든 여기서 끝난다
		BeginDestroyForPendingAction();
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



    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("HOST SESSION NONE"));
    }

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

		bool bHasRoomCode = Result.Session.SessionSettings.Get(FName(TEXT("ROOM_CODE")), RoomCode);
		SessionDebug(TEXT("ROOM_CODE Found"), bHasRoomCode);



		//임시 제거
        // 비공개 방 제외
        if (bHasRoomCode)
        {
            continue;
        }


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

	JoinDebug(SearchIndex);

	if (!SessionInterface.IsValid())
	{
		SessionDebug(TEXT("Join: SessionInterface 없음"), false);
		return;
	}

	// 인덱스는 UI 가 넘겨준다. 목록이 갱신된 뒤 낡은 인덱스가 들어오면 그대로 크래시라
	// 배열 접근 전에 막는다 (아래에서 PublicSessionResults[SearchIndex] 를 그대로 쓴다)
	if (!PublicSessionResults.IsValidIndex(SearchIndex))
	{
		SessionDebug(FString::Printf(TEXT("Join: 잘못된 인덱스 %d / 목록 %d개"),
			SearchIndex, PublicSessionResults.Num()), false);
		return;
	}

	// 기존 GameSession 이 남아 있으면 JoinSession 이 시작조차 되지 않는다 —
	// "already exists, can't join twice" 로 거절당하고 Result 5(UnknownError)만 돌아온다.
	// 방을 한 번 만들었거나 한 번 참가했던 인스턴스는 그 뒤로 아무 방에도 못 들어가게 된다.
	//
	// 생성 경로와 같은 처리다 — 먼저 지우고, 완료 콜백에서 이어서 참가한다
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		SessionDebug(TEXT("Join: 기존 세션을 지우고 이어서 참가한다"), true);

		PendingAction = EPendingSessionAction::Join;
		PendingJoinIndex = SearchIndex;

		BeginDestroyForPendingAction();
		return;
	}

	// 삭제 후 재시도로 이 함수가 다시 불릴 수 있다 (위 pending 경로)
	if (JoinHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		JoinHandle.Reset();
	}

    // Join 완료 델리게이트 등록
    JoinHandle =
        SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
            FOnJoinSessionCompleteDelegate::CreateUObject(
                this,
                &ATitlePlayerController::TitleOnJoinSessionComplete));


	SessionDebug("Join: Delegate Added",true);


	// 선택한 세션 검색 결과 가져오기
    const FOnlineSessionSearchResult& Result =
        PublicSessionResults[SearchIndex];

    // 세션 ID 확인
	SessionDebug((TEXT("Join: Session ID = %s"), *Result.GetSessionIdStr()), true);



	// 방 제목 가져오기
	FString RoomName;

	bool bGotRoomName =
		Result.Session.SessionSettings.Get(
			FName("ROOM_NAME"),
			RoomName
		);


	SessionDebug((TEXT("Get Room Name = %s"), *RoomName), bGotRoomName);

	// NetGameInstanceSubsystem에 저장
	UNetGameInstanceSubsystem* NetSubsystem =
		GetGameInstance()->GetSubsystem<UNetGameInstanceSubsystem>();

	if (NetSubsystem)
	{
		NetSubsystem->JoinedRoomName = RoomName;
		NetSubsystem->MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
	}



    // 선택한 세션에 참가 요청
    bool bStarted = SessionInterface->JoinSession(0, NAME_GameSession, Result);

    // JoinSession 호출 자체가 성공했는지 확인
	SessionDebug("JoinSession SUCCESS?", bStarted);

	// 세션 참가 요청 종료
	SessionDebug("===== Join Session End =====", true);
 

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


	// Join 성공 여부
    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Error,
            TEXT("JOIN FAILED"));
        return;
    }


	// ++
	// Join한 Session 가져오기
	FNamedOnlineSession* JoinedSession =
		SessionInterface->GetNamedSession(SessionName);

	if (!JoinedSession)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Joined Session is invalid"));

		return;
	}








	// Connect String 가져오기
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


	// 이동
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
}

void ATitlePlayerController::JoinDebug(int32 SearchIndex)
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

}

bool ATitlePlayerController::BeginDestroyForPendingAction()
{
	if (!SessionInterface.IsValid())
	{
		PendingAction = EPendingSessionAction::None;
		return false;
	}

	// 이전 시도의 델리게이트가 남아 있으면 콜백이 두 번 불린다
	if (DestroyHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		DestroyHandle.Reset();
	}

	DestroyHandle =
		SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(
				this,
				&ATitlePlayerController::TitleOnDestroySessionComplete));

	SessionDebug(TEXT("Destroy Delegate Added"), DestroyHandle.IsValid());

	if (!DestroyHandle.IsValid())
	{
		PendingAction = EPendingSessionAction::None;
		return false;
	}

	const bool bDestroyStarted = SessionInterface->DestroySession(NAME_GameSession);

	SessionDebug(TEXT("DestroySession"), bDestroyStarted);

	// 시작조차 못 했으면 완료 콜백이 오지 않는다. 예약을 남겨 두면 다음 삭제 때
	// 엉뚱하게 되살아나므로 여기서 되돌린다
	if (!bDestroyStarted)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		DestroyHandle.Reset();
		PendingAction = EPendingSessionAction::None;
	}

	return bDestroyStarted;
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

	// 예약을 **먼저 비운다.** 아래에서 다시 부르는 함수들이 기존 세션을 또 발견하면
	// 무한히 삭제를 반복하게 되는데, 예약이 비어 있으면 그 고리가 끊긴다
	const EPendingSessionAction Resumed = PendingAction;
	PendingAction = EPendingSessionAction::None;

	if (!bWasSuccessful)
	{
		SessionDebug(TEXT("삭제 실패 — 예약된 작업을 취소한다"), false);
		return;
	}

	switch (Resumed)
	{
	case EPendingSessionAction::Create:
		SessionDebug(TEXT("Creating New Session"), true);
		TitleCreateSession(PendingRoomName, PendingMaxPlayer, bPendingIsPublic);
		break;

	case EPendingSessionAction::Join:
		SessionDebug(FString::Printf(TEXT("이어서 참가 — 인덱스 %d"), PendingJoinIndex), true);
		TitleJoinSession(PendingJoinIndex);
		PendingJoinIndex = INDEX_NONE;
		break;

	case EPendingSessionAction::None:
	default:
		// 우리가 예약하지 않은 삭제다(직접 호출 등). 이어서 할 일이 없다
		break;
	}
}
