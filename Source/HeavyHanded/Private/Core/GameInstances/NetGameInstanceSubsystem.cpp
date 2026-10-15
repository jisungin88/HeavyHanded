// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameInstances/NetGameInstanceSubsystem.h"
//#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"


FString UNetGameInstanceSubsystem::GetJoinedRoomName() const
{
	return JoinedRoomName;
}

FString UNetGameInstanceSubsystem::GetJoinedRoomCode() const
{
	return JoinedRoomCode;
}

int32 UNetGameInstanceSubsystem::GetRoomMaxPlayer() const
{
	return MaxPlayers;
}




// ------------------------------ 디버그 로그 ------------------------------

// 디버그 메시지를 로그 배열에 저장하고 Output Log에 출력
// bError가 true면 Error 로그, false면 Warning 로그로 출력
void UNetGameInstanceSubsystem::DebugInstMessage(const FString& Message, bool bError)
{
	DebugLogs.Add(Message);

	if (bError)
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	}
}

void UNetGameInstanceSubsystem::SetDebugErrorWidgetClass(TSubclassOf<UUserWidget> WidgetClass)
{
	DebugErrorWidgetClass = WidgetClass;
}

// 디버그 에러 UI를 생성하고 저장된 로그를 TextBlock에 표시
void UNetGameInstanceSubsystem::ShowDebugErrorUI(const TArray<FString>& Logs)
{
	// 디버그 UI 클래스가 지정되어 있는지 확인
	if (!DebugErrorWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugErrorWidgetClass가 설정되지 않았습니다."));
		return;
	}

	// 현재 월드가 존재하는지 확인
	UWorld* World = GetWorld();

	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] World를 찾을 수 없습니다."));
		return;
	}

	// 디버그 에러 위젯 생성
	UUserWidget* DebugWidget = CreateWidget<UUserWidget>(World, DebugErrorWidgetClass);

	if (!DebugWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugErrorWidget 생성에 실패했습니다."));
		return;
	}

	// 로그 배열을 하나의 문자열로 합침
	FString CombinedLogs;

	for (const FString& Log : Logs)
	{
		CombinedLogs += Log;
		CombinedLogs += TEXT("\n");
	}

	// 위젯 내부에서 이름이 DebugLogText인 TextBlock 찾기
	UTextBlock* DebugLogText = Cast<UTextBlock>(
		DebugWidget->GetWidgetFromName(TEXT("DebugLogText"))
	);

	if (!DebugLogText)
	{
		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugLogText를 찾을 수 없습니다."));
		DebugWidget->RemoveFromParent();
		return;
	}

	// TextBlock에 로그 출력
	DebugLogText->SetText(FText::FromString(CombinedLogs));

	// 최상위 레이어에 UI 표시
	DebugWidget->AddToViewport(9999);

	UE_LOG(LogTemp, Warning, TEXT("[DebugUI] 디버그 에러 UI를 표시했습니다."));
}



// 에러 발생 정보를 저장하고 Output Log에 에러 내용 출력
// ErrorCode : 에러 종류
// Context   : 에러가 발생한 함수 또는 시스템
// Message   : 구체적인 에러 원인
void UNetGameInstanceSubsystem::RecordError(const FString& ErrorCode)
{
	// DataTable이 없으면 종료
	if (!ErrorCodeTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunError] ErrorCodeTable이 설정되지 않았습니다."));
		return;
	}

	// ErrorCode에 해당하는 DataTable Row 찾기
	const FDebugErrorInfo* ErrorData = ErrorCodeTable->FindRow<FDebugErrorInfo>(
		FName(*ErrorCode),
		TEXT("RecordError")
	);

	// 해당 에러 코드가 없으면 종료
	if (!ErrorData)
	{
		UE_LOG(LogTemp, Error, TEXT("[RunError] 존재하지 않는 에러 코드: %s"), *ErrorCode);
		return;
	}

	// DataTable의 정보를 마지막 에러로 저장
	LastError = *ErrorData;

	// 로그 생성
	const FString FullMessage = FString::Printf(
		TEXT("[%s] [%s] %s | Developer: %s"),
		*ErrorData->ErrorCode,
		*ErrorData->Context,
		*ErrorData->Message,
		*ErrorData->DeveloperMessage.ToString()
	);

	// 로그 저장
	DebugLogs.Add(FullMessage);

	// Output Log 출력
	UE_LOG(LogTemp, Error, TEXT("[RunError] %s"), *FullMessage);
}


// ------------------------------ 세션 C++ 이전 작업 (보류, 2026-10-12) ------------------------------
// #include "Core/GameInstances/NetGameInstanceSubsystem.h"
// #include "Core/GameInstances/NetSessionSettings.h"
// #include "Components/TextBlock.h"
// #include "Engine/Engine.h"
// #include "Engine/World.h"
// #include "GameFramework/PlayerController.h"
// #include "Kismet/GameplayStatics.h"
// #include "OnlineSubsystemUtils.h"
//
// DEFINE_LOG_CATEGORY_STATIC(LogSession, Log, All);
//
// namespace
// {
// 	const FName RoomNameKey(TEXT("ROOM_NAME"));
// 	const FName RoomCodeKey(TEXT("ROOM_CODE"));
// }
//
// // ------------------------------ 방 정보 ------------------------------
//
// FString UNetGameInstanceSubsystem::GetJoinedRoomName() const
// {
// 	return JoinedRoomName;
// }
//
// FString UNetGameInstanceSubsystem::GetJoinedRoomCode() const
// {
// 	return JoinedRoomCode;
// }
//
// int32 UNetGameInstanceSubsystem::GetRoomMaxPlayer() const
// {
// 	return MaxPlayers;
// }
//
//
//
//
// // ------------------------------ 디버그 로그 ------------------------------
//
// // 디버그 메시지를 로그 배열에 저장하고 Output Log에 출력
// // bError가 true면 Error 로그, false면 Warning 로그로 출력
// void UNetGameInstanceSubsystem::DebugInstMessage(const FString& Message, bool bError)
// {
// 	DebugLogs.Add(Message);
//
// 	if (bError)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
// 	}
// 	else
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
// 	}
// }
//
// void UNetGameInstanceSubsystem::SetDebugErrorWidgetClass(TSubclassOf<UUserWidget> WidgetClass)
// {
// 	DebugErrorWidgetClass = WidgetClass;
// }
//
// // 디버그 에러 UI를 생성하고 저장된 로그를 TextBlock에 표시
// void UNetGameInstanceSubsystem::ShowDebugErrorUI(const TArray<FString>& Logs)
// {
// 	// 디버그 UI 클래스가 지정되어 있는지 확인
// 	if (!DebugErrorWidgetClass)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugErrorWidgetClass가 설정되지 않았습니다."));
// 		return;
// 	}
//
// 	// 현재 월드가 존재하는지 확인
// 	UWorld* World = GetWorld();
//
// 	if (!World)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("[DebugUI] World를 찾을 수 없습니다."));
// 		return;
// 	}
//
// 	// 디버그 에러 위젯 생성
// 	UUserWidget* DebugWidget = CreateWidget<UUserWidget>(World, DebugErrorWidgetClass);
//
// 	if (!DebugWidget)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugErrorWidget 생성에 실패했습니다."));
// 		return;
// 	}
//
// 	// 로그 배열을 하나의 문자열로 합침
// 	FString CombinedLogs;
//
// 	for (const FString& Log : Logs)
// 	{
// 		CombinedLogs += Log;
// 		CombinedLogs += TEXT("\n");
// 	}
//
// 	// 위젯 내부에서 이름이 DebugLogText인 TextBlock 찾기
// 	UTextBlock* DebugLogText = Cast<UTextBlock>(
// 		DebugWidget->GetWidgetFromName(TEXT("DebugLogText"))
// 	);
//
// 	if (!DebugLogText)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("[DebugUI] DebugLogText를 찾을 수 없습니다."));
// 		DebugWidget->RemoveFromParent();
// 		return;
// 	}
//
// 	// TextBlock에 로그 출력
// 	DebugLogText->SetText(FText::FromString(CombinedLogs));
//
// 	// 최상위 레이어에 UI 표시
// 	DebugWidget->AddToViewport(9999);
//
// 	UE_LOG(LogTemp, Warning, TEXT("[DebugUI] 디버그 에러 UI를 표시했습니다."));
// }
//
//
//
// // 에러 발생 정보를 저장하고 Output Log에 에러 내용 출력
// // ErrorCode : 에러 종류
// // Context   : 에러가 발생한 함수 또는 시스템
// // Message   : 구체적인 에러 원인
// void UNetGameInstanceSubsystem::RecordError(const FString& ErrorCode)
// {
// 	// DataTable이 없으면 종료
// 	if (!ErrorCodeTable)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("[RunError] ErrorCodeTable이 설정되지 않았습니다."));
// 		return;
// 	}
//
// 	// ErrorCode에 해당하는 DataTable Row 찾기
// 	const FDebugErrorInfo* ErrorData = ErrorCodeTable->FindRow<FDebugErrorInfo>(
// 		FName(*ErrorCode),
// 		TEXT("RecordError")
// 	);
//
// 	// 해당 에러 코드가 없으면 종료
// 	if (!ErrorData)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("[RunError] 존재하지 않는 에러 코드: %s"), *ErrorCode);
// 		return;
// 	}
//
// 	// DataTable의 정보를 마지막 에러로 저장
// 	LastError = *ErrorData;
//
// 	// 로그 생성
// 	const FString FullMessage = FString::Printf(
// 		TEXT("[%s] [%s] %s | Developer: %s"),
// 		*ErrorData->ErrorCode,
// 		*ErrorData->Context,
// 		*ErrorData->Message,
// 		*ErrorData->DeveloperMessage.ToString()
// 	);
//
// 	// 로그 저장
// 	DebugLogs.Add(FullMessage);
//
// 	// Output Log 출력
// 	UE_LOG(LogTemp, Error, TEXT("[RunError] %s"), *FullMessage);
// }
//
//
// // ------------------------------ 수명 ------------------------------
//
// void UNetGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
// {
// 	Super::Initialize(Collection);
//
// 	if (GEngine)
// 	{
// 		NetFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
// 		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
// 	}
// }
//
// void UNetGameInstanceSubsystem::Deinitialize()
// {
// 	if (GEngine)
// 	{
// 		GEngine->OnNetworkFailure().Remove(NetFailureHandle);
// 		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
// 	}
//
// 	ClearOpDelegates();
//
// 	Super::Deinitialize();
// }
//
// IOnlineSessionPtr UNetGameInstanceSubsystem::GetSessionInterface() const
// {
// 	// PIE 창마다 OSS 인스턴스가 다르다. IOnlineSubsystem::Get() 은 기본 인스턴스만 준다
// 	const UGameInstance* GI = GetGameInstance();
// 	return Online::GetSessionInterface(GI ? GI->GetWorld() : nullptr);
// }
//
// bool UNetGameInstanceSubsystem::HasGameSession() const
// {
// 	const IOnlineSessionPtr Sessions = GetSessionInterface();
// 	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
// }
//
//
// // ------------------------------ 생성 ------------------------------
//
// void UNetGameInstanceSubsystem::CreateRoom(const FString& RoomName, int32 MaxPlayer, bool bIsPublic)
// {
// 	if (IsBusy())
// 	{
// 		OnCreateRoomComplete.Broadcast(ESessionOpResult::Busy);
// 		return;
// 	}
//
// 	const UNetSessionSettings* Settings = UNetSessionSettings::Get();
//
// 	PendingRoomName = RoomName.TrimStartAndEnd();
// 	PendingMaxPlayer = FMath::Clamp(MaxPlayer, Settings->MinPlayers, Settings->MaxPlayers);
// 	bPendingIsPublic = bIsPublic;
// 	CurrentOp = ESessionOp::Create;
//
// 	UE_LOG(LogSession, Log, TEXT("CreateRoom: %s, %d, Public=%d"), *PendingRoomName, PendingMaxPlayer, bIsPublic);
//
// 	// 남은 세션이 있으면 생성이 거절된다. 지운 뒤 이어서 만든다
// 	if (HasGameSession())
// 	{
// 		BeginDestroySession(ESessionOp::Create);
// 		return;
// 	}
//
// 	StartCreate();
// }
//
// void UNetGameInstanceSubsystem::StartCreate()
// {
// 	const IOnlineSessionPtr Sessions = GetSessionInterface();
// 	if (!Sessions.IsValid())
// 	{
// 		FinishOp(ESessionOp::Create, ESessionOpResult::NoInterface);
// 		return;
// 	}
//
// 	const UNetSessionSettings* Settings = UNetSessionSettings::Get();
//
// 	FOnlineSessionSettings SessionSettings;
// 	SessionSettings.bIsLANMatch = true;
// 	SessionSettings.NumPublicConnections = PendingMaxPlayer;
// 	SessionSettings.bShouldAdvertise = true;
// 	SessionSettings.bAllowJoinInProgress = true;
// 	SessionSettings.bUsesPresence = false;
//
// 	const FString RoomName = PendingRoomName.IsEmpty()
// 			? FString::Printf(TEXT("%s_%s"), *Settings->DefaultRoomNamePrefix,
// 					*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(Settings->RandomNameSuffixLength))
// 			: PendingRoomName;
//
// 	SessionSettings.Set(RoomNameKey, RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
//
// 	FString RoomCode;
// 	if (!bPendingIsPublic)
// 	{
// 		RoomCode = GenerateRoomCode();
// 		SessionSettings.Set(RoomCodeKey, RoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
// 	}
//
// 	JoinedRoomName = RoomName;
// 	JoinedRoomCode = RoomCode;
// 	MaxPlayers = PendingMaxPlayer;
//
// 	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
// 			FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateComplete));
//
// 	// Null 은 완료 콜백을 호출 안에서 바로 부른다. 핸들이 비어 있으면 이미 처리된 것이다
// 	if (!Sessions->CreateSession(0, NAME_GameSession, SessionSettings) && CreateHandle.IsValid())
// 	{
// 		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
// 		ClearRoomInfo();
// 		FinishOp(ESessionOp::Create, ESessionOpResult::Failed);
// 	}
// }
//
// void UNetGameInstanceSubsystem::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
// {
// 	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
// 	{
// 		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
// 	}
// 	CreateHandle.Reset();
//
// 	UE_LOG(LogSession, Log, TEXT("CreateComplete: %d"), bWasSuccessful);
//
// 	if (!bWasSuccessful)
// 	{
// 		ClearRoomInfo();
// 		FinishOp(ESessionOp::Create, ESessionOpResult::Failed);
// 		return;
// 	}
//
// 	FinishOp(ESessionOp::Create, ESessionOpResult::Success);
// 	TravelToMap(UNetSessionSettings::Get()->ShelterMap, TEXT("listen"));
// }
//
//
// // ------------------------------ 검색 ------------------------------
//
// void UNetGameInstanceSubsystem::RefreshRoomList()
// {
// 	if (IsBusy())
// 	{
// 		OnRoomListUpdated.Broadcast(ESessionOpResult::Busy);
// 		return;
// 	}
//
// 	bSearchFiltered = false;
// 	PendingKeyword.Reset();
// 	CurrentOp = ESessionOp::FindList;
// 	StartFind();
// }
//
// void UNetGameInstanceSubsystem::SearchRooms(ERoomSearchType SearchType, const FString& Keyword)
// {
// 	const FString Trimmed = Keyword.TrimStartAndEnd();
// 	if (Trimmed.IsEmpty())
// 	{
// 		RefreshRoomList();
// 		return;
// 	}
//
// 	if (IsBusy())
// 	{
// 		OnRoomListUpdated.Broadcast(ESessionOpResult::Busy);
// 		return;
// 	}
//
// 	// Null 은 검색 조건으로 걸러 주지 않는다. 전부 받아서 로컬에서 비교한다
// 	bSearchFiltered = true;
// 	PendingSearchType = SearchType;
// 	PendingKeyword = Trimmed;
// 	CurrentOp = ESessionOp::FindList;
// 	StartFind();
// }
//
// bool UNetGameInstanceSubsystem::MatchesSearch(const FOnlineSessionSearchResult& Result) const
// {
// 	FString Code;
// 	const bool bPrivate = Result.Session.SessionSettings.Get(RoomCodeKey, Code);
//
// 	if (!bSearchFiltered)
// 	{
// 		return !bPrivate;
// 	}
//
// 	if (PendingSearchType == ERoomSearchType::RoomCode)
// 	{
// 		return bPrivate && Code.Equals(PendingKeyword, ESearchCase::IgnoreCase);
// 	}
//
// 	FString RoomName;
// 	Result.Session.SessionSettings.Get(RoomNameKey, RoomName);
// 	return !bPrivate && RoomName.Contains(PendingKeyword, ESearchCase::IgnoreCase);
// }
//
// void UNetGameInstanceSubsystem::StartFind()
// {
// 	const IOnlineSessionPtr Sessions = GetSessionInterface();
// 	if (!Sessions.IsValid())
// 	{
// 		FinishOp(CurrentOp, ESessionOpResult::NoInterface);
// 		return;
// 	}
//
// 	SessionSearch = MakeShared<FOnlineSessionSearch>();
// 	SessionSearch->bIsLanQuery = true;
// 	SessionSearch->MaxSearchResults = UNetSessionSettings::Get()->MaxSearchResults;
//
// 	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
// 			FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindComplete));
//
// 	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()) && FindHandle.IsValid())
// 	{
// 		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
// 		FinishOp(CurrentOp, ESessionOpResult::Failed);
// 	}
// }
//
// void UNetGameInstanceSubsystem::HandleFindComplete(bool bWasSuccessful)
// {
// 	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
// 	{
// 		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
// 	}
// 	FindHandle.Reset();
//
// 	const ESessionOp Op = CurrentOp;
//
// 	if (!bWasSuccessful || !SessionSearch.IsValid())
// 	{
// 		UE_LOG(LogSession, Warning, TEXT("FindComplete: 실패"));
// 		FinishOp(Op, ESessionOpResult::Failed);
// 		return;
// 	}
//
// 	UE_LOG(LogSession, Log, TEXT("FindComplete: %d개"), SessionSearch->SearchResults.Num());
//
// 	RoomList.Reset();
// 	CachedResults.Reset();
//
// 	for (const FOnlineSessionSearchResult& Result : SessionSearch->SearchResults)
// 	{
// 		if (!Result.IsValid() || !MatchesSearch(Result))
// 		{
// 			continue;
// 		}
//
// 		FRoomListData Room;
// 		Room.SessionId = Result.GetSessionIdStr();
// 		Result.Session.SessionSettings.Get(RoomNameKey, Room.RoomName);
// 		Room.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
// 		Room.CurrentPlayers = Room.MaxPlayers - Result.Session.NumOpenPublicConnections;
//
// 		RoomList.Add(Room);
// 		CachedResults.Add(Result);
// 	}
//
// 	// 전체 목록이 비면 Success(방 없음), 검색 결과가 비면 NotFound
// 	const bool bNotFound = bSearchFiltered && RoomList.IsEmpty();
// 	FinishOp(ESessionOp::FindList, bNotFound ? ESessionOpResult::NotFound : ESessionOpResult::Success);
// }
//
//
// // ------------------------------ 참가 ------------------------------
//
// void UNetGameInstanceSubsystem::JoinRoomById(const FString& SessionId)
// {
// 	if (IsBusy())
// 	{
// 		OnJoinRoomComplete.Broadcast(ESessionOpResult::Busy);
// 		return;
// 	}
//
// 	const FOnlineSessionSearchResult* Found = CachedResults.FindByPredicate(
// 			[&SessionId](const FOnlineSessionSearchResult& Result)
// 			{
// 					return Result.GetSessionIdStr() == SessionId;
// 			});
//
// 	if (!Found)
// 	{
// 		OnJoinRoomComplete.Broadcast(ESessionOpResult::NotFound);
// 		return;
// 	}
//
// 	CurrentOp = ESessionOp::Join;
// 	BeginJoin(*Found);
// }
//
// void UNetGameInstanceSubsystem::BeginJoin(const FOnlineSessionSearchResult& Result)
// {
// 	PendingJoinResult = Result;
//
// 	// 남은 세션이 있으면 "already exists" 로 거절되고 Result 5 만 돌아온다
// 	if (HasGameSession())
// 	{
// 		BeginDestroySession(ESessionOp::Join);
// 		return;
// 	}
//
// 	StartJoin();
// }
//
// void UNetGameInstanceSubsystem::StartJoin()
// {
// 	const IOnlineSessionPtr Sessions = GetSessionInterface();
// 	if (!Sessions.IsValid())
// 	{
// 		FinishOp(ESessionOp::Join, ESessionOpResult::NoInterface);
// 		return;
// 	}
//
// 	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
// 			FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinComplete));
//
// 	if (!Sessions->JoinSession(0, NAME_GameSession, PendingJoinResult) && JoinHandle.IsValid())
// 	{
// 		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
// 		FinishOp(ESessionOp::Join, ESessionOpResult::Failed);
// 	}
// }
//
// void UNetGameInstanceSubsystem::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
// {
// 	const IOnlineSessionPtr Sessions = GetSessionInterface();
// 	if (Sessions.IsValid())
// 	{
// 		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
// 	}
// 	JoinHandle.Reset();
//
// 	UE_LOG(LogSession, Log, TEXT("JoinComplete: %d"), static_cast<int32>(Result));
//
// 	if (Result != EOnJoinSessionCompleteResult::Success)
// 	{
// 		const ESessionOpResult OpResult =
// 				Result == EOnJoinSessionCompleteResult::SessionIsFull ? ESessionOpResult::Full
// 				: Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? ESessionOpResult::NotFound
// 				: ESessionOpResult::Failed;
//
// 		FinishOp(ESessionOp::Join, OpResult);
// 		return;
// 	}
//
// 	FString ConnectString;
// 	if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(SessionName, ConnectString) || ConnectString.IsEmpty())
// 	{
// 		// 세션만 남고 이동을 못 하면 다음 참가가 막힌다
// 		FinishOp(ESessionOp::Join, ESessionOpResult::Failed);
// 		CurrentOp = ESessionOp::Cleanup;
// 		BeginDestroySession(ESessionOp::None);
// 		return;
// 	}
//
// 	const FOnlineSessionSettings& Joined = PendingJoinResult.Session.SessionSettings;
// 	JoinedRoomName.Reset();
// 	JoinedRoomCode.Reset();
// 	Joined.Get(RoomNameKey, JoinedRoomName);
// 	Joined.Get(RoomCodeKey, JoinedRoomCode);
// 	MaxPlayers = Joined.NumPublicConnections;
//
// 	FinishOp(ESessionOp::Join, ESessionOpResult::Success);
//
// 	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
// 	if (!PC)
// 	{
// 		UE_LOG(LogSession, Error, TEXT("JoinComplete: 로컬 PC 없음"));
// 		return;
// 	}
//
// 	PC->ClientTravel(ConnectString, TRAVEL_Absolute);
// }
//
// // ------------------------------ 나가기 ------------------------------
//
// void UNetGameInstanceSubsystem::LeaveRoom()
// {
// 	if (IsBusy())
// 	{
// 		OnLeaveRoomComplete.Broadcast(ESessionOpResult::Busy);
// 		return;
// 	}
//
// 	CurrentOp = ESessionOp::Leave;
//
// 	if (!HasGameSession())
// 	{
// 		FinishLeave();
// 		return;
// 	}
//
// 	BeginDestroySession(ESessionOp::Leave);
// }
//
// void UNetGameInstanceSubsystem::FinishLeave()
// {
// 	ClearRoomInfo();
// 	FinishOp(ESessionOp::Leave, ESessionOpResult::Success);
// 	TravelToMap(UNetSessionSettings::Get()->TitleMap, FString());
// }
//
//
// // ------------------------------ 삭제 후 재시도 ------------------------------
//
// void UNetGameInstanceSubsystem::BeginDestroySession(ESessionOp After)
// {
// 	const IOnlineSessionPtr Sessions = GetSessionInterface();
// 	if (!Sessions.IsValid())
// 	{
// 		After == ESessionOp::Leave ? FinishLeave() : FinishOp(CurrentOp, ESessionOpResult::NoInterface);
// 		return;
// 	}
//
// 	PendingAfterDestroy = After;
//
// 	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
// 			FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroyComplete));
//
// 	if (!Sessions->DestroySession(NAME_GameSession) && DestroyHandle.IsValid())
// 	{
// 		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
// 		PendingAfterDestroy = ESessionOp::None;
//
// 		// 나가기는 삭제가 실패해도 타이틀로 보낸다
// 		After == ESessionOp::Leave ? FinishLeave() : FinishOp(CurrentOp, ESessionOpResult::Failed);
// 	}
// }
//
// void UNetGameInstanceSubsystem::HandleDestroyComplete(FName SessionName, bool bWasSuccessful)
// {
// 	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
// 	{
// 		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
// 	}
// 	DestroyHandle.Reset();
//
// 	UE_LOG(LogSession, Log, TEXT("DestroyComplete: %d"), bWasSuccessful);
//
// 	// 예약을 먼저 비운다. Start* 는 다시 삭제하지 않으므로 반복되지 않는다
// 	const ESessionOp After = PendingAfterDestroy;
// 	PendingAfterDestroy = ESessionOp::None;
//
// 	if (After == ESessionOp::Leave)
// 	{
// 		FinishLeave();
// 		return;
// 	}
//
// 	if (!bWasSuccessful)
// 	{
// 		FinishOp(CurrentOp, ESessionOpResult::Failed);
// 		return;
// 	}
//
// 	switch (After)
// 	{
// 	case ESessionOp::Create:
// 		StartCreate();
// 		break;
//
// 	case ESessionOp::Join:
// 		StartJoin();
// 		break;
//
// 	default:
// 		FinishOp(CurrentOp, ESessionOpResult::Success);
// 		break;
// 	}
// }
//
//
// // ------------------------------ 연결 실패 ------------------------------
//
// void UNetGameInstanceSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
// 	  ENetworkFailure::Type FailureType, const FString& ErrorString)
// {
// 	// PIE 에서는 다른 창의 실패도 여기로 온다
// 	if (World && World->GetGameInstance() != GetGameInstance())
// 	{
// 		return;
// 	}
//
// 	// 호스트는 클라 한 명이 끊겨도 받을 수 있다. 방을 지우면 안 된다
// 	if (World && World->GetNetMode() == NM_ListenServer)
// 	{
// 		return;
// 	}
//
// 	UE_LOG(LogSession, Warning, TEXT("NetworkFailure: %s %s"), ENetworkFailure::ToString(FailureType), *ErrorString);
// 	RecoverFromFailure();
// }
//
// void UNetGameInstanceSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
// {
// 	if (World && World->GetGameInstance() != GetGameInstance())
// 	{
// 		return;
// 	}
//
// 	UE_LOG(LogSession, Warning, TEXT("TravelFailure: %s %s"), ETravelFailure::ToString(FailureType), *ErrorString);
// 	RecoverFromFailure();
// }
//
// void UNetGameInstanceSubsystem::RecoverFromFailure()
// {
// 	const ESessionOp Interrupted = CurrentOp;
//
// 	ClearOpDelegates();
//
// 	if (Interrupted != ESessionOp::None)
// 	{
// 		FinishOp(Interrupted, ESessionOpResult::Failed);
// 	}
//
// 	ClearRoomInfo();
//
// 	// 타이틀 복귀는 엔진이 한다. 남은 세션만 지운다
// 	if (HasGameSession())
// 	{
// 		CurrentOp = ESessionOp::Cleanup;
// 		BeginDestroySession(ESessionOp::None);
// 	}
// }
//
//
// // ------------------------------ 공통 ------------------------------
//
// void UNetGameInstanceSubsystem::FinishOp(ESessionOp Op, ESessionOpResult Result)
// {
// 	CurrentOp = ESessionOp::None;
// 	PendingAfterDestroy = ESessionOp::None;
//
// 	UE_LOG(LogSession, Log, TEXT("FinishOp: Op=%d Result=%d"), static_cast<int32>(Op), static_cast<int32>(Result));
//
// 	switch (Op)
// 	{
// 	case ESessionOp::Create:
// 		OnCreateRoomComplete.Broadcast(Result);
// 		break;
//
// 	case ESessionOp::FindList:
// 		OnRoomListUpdated.Broadcast(Result);
// 		break;
//
// 	case ESessionOp::Join:
// 		OnJoinRoomComplete.Broadcast(Result);
// 		break;
//
// 	case ESessionOp::Leave:
// 		OnLeaveRoomComplete.Broadcast(Result);
// 		break;
//
// 	default:
// 		break;
// 	}
// }
//
// void UNetGameInstanceSubsystem::ClearOpDelegates()
// {
// 	if (const IOnlineSessionPtr Sessions = GetSessionInterface())
// 	{
// 		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
// 		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
// 		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
// 		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
// 	}
//
// 	CreateHandle.Reset();
// 	FindHandle.Reset();
// 	JoinHandle.Reset();
// 	DestroyHandle.Reset();
// 	PendingAfterDestroy = ESessionOp::None;
// }
//
// FString UNetGameInstanceSubsystem::GenerateRoomCode() const
// {
// 	const UNetSessionSettings* Settings = UNetSessionSettings::Get();
// 	const FString& Chars = Settings->RoomCodeChars;
//
// 	FString Code;
// 	if (Chars.IsEmpty())
// 	{
// 		return Code;
// 	}
//
// 	for (int32 i = 0; i < Settings->RoomCodeLength; ++i)
// 	{
// 		Code.AppendChar(Chars[FMath::RandRange(0, Chars.Len() - 1)]);
// 	}
//
// 	return Code;
// }
//
// void UNetGameInstanceSubsystem::ClearRoomInfo()
// {
// 	JoinedRoomName.Reset();
// 	JoinedRoomCode.Reset();
// 	MaxPlayers = 0;
// }
//
// void UNetGameInstanceSubsystem::TravelToMap(const TSoftObjectPtr<UWorld>& Map, const FString& Options)
// {
// 	if (Map.IsNull())
// 	{
// 		UE_LOG(LogSession, Error, TEXT("이동할 맵이 비어 있다. Project Settings > Game > Session 확인"));
// 		return;
// 	}
//
// 	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Map, true, Options);
// }
