// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Subsystems/GameInstanceSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"

#include "NetGameInstanceSubsystem.generated.h"

/**
 * 
 */

class UTextBlock;

USTRUCT(BlueprintType, Category = "Debug")
struct FDebugErrorInfo : public FTableRowBase
{
	GENERATED_BODY()

	// 에러 코드
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString ErrorCode;

	// 에러가 발생한 시스템 또는 함수
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Context;

	// 사용자에게 보여줄 에러 메시지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Message;

	// 개발자용 상세 에러 메시지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DeveloperMessage;
};



UCLASS()
class HEAVYHANDED_API UNetGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()


public:

	// -------------------------- 방 정보 설정 -----------------------------------

	// 현재 참가한 방 제목
	UPROPERTY(BlueprintReadWrite, Category = "Room")
	FString JoinedRoomName;

	UPROPERTY(BlueprintReadWrite, Category = "Room")
	FString JoinedRoomCode;

	UPROPERTY(BlueprintReadWrite, Category = "Room")
	int32 MaxPlayers = 0;


	UFUNCTION(BlueprintPure, Category = "Room")
	FString GetJoinedRoomName() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	FString GetJoinedRoomCode() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	int32 GetRoomMaxPlayer() const;




	// ------------------------------ 디버그용 -----------------------------------

public:

	// 디버그 메시지를 저장하고 Output Log에 출력
	// bError가 true면 Error 로그로 출력
	// bError가 false면 Warning 로그로 출력
	void DebugInstMessage(const FString& Message, bool bError);


	// 현재까지 저장된 모든 디버그 로그 반환
	const TArray<FString>& GetDebugLogs() const
	{
		return DebugLogs;
	}


	// 저장된 디버그 로그 전체 삭제
	void ClearDebugLogs()
	{
		DebugLogs.Empty();
	}


	// ------------------------------ 디버그 UI -----------------------------------

	// 디버그 에러 UI로 사용할 위젯 클래스
	// 블루프린트에서 WBP_DebugError 같은 위젯을 지정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
	TSubclassOf<UUserWidget> DebugErrorWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void SetDebugErrorWidgetClass(TSubclassOf<UUserWidget> WidgetClass);


	// 저장된 디버그 로그를 UI에 표시
	void ShowDebugErrorUI(const TArray<FString>& Logs);


	// ------------------------------ 에러 정보 -----------------------------------

	// 에러 코드가 저장된 DataTable
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	UDataTable* ErrorCodeTable;

	// 에러 코드에 해당하는 정보를 DataTable에서 찾아 저장
	void RecordError(const FString& ErrorCode);


	// 마지막으로 기록된 에러 정보 반환
	const FDebugErrorInfo& GetLastError() const
	{
		return LastError;
	}


private:

	// 마지막으로 발생한 에러 정보
	UPROPERTY()
	FDebugErrorInfo LastError;


	// 지금까지 저장된 디버그 로그
	UPROPERTY()
	TArray<FString> DebugLogs;


};


// ------------------------------ 세션 C++ 이전 작업 (보류, 2026-10-12) ------------------------------
// #pragma once

// #include "CoreMinimal.h"

// #include "Subsystems/GameInstanceSubsystem.h"
// #include "Blueprint/UserWidget.h"
// #include "Engine/DataTable.h"
// #include "Engine/EngineBaseTypes.h"
// #include "Interfaces/OnlineSessionInterface.h"
// #include "OnlineSessionSettings.h"

// #include "NetGameInstanceSubsystem.generated.h"
//
// class UTextBlock;
// class UNetDriver;
//
// USTRUCT(BlueprintType, Category = "Debug")
// struct FDebugErrorInfo : public FTableRowBase
// {
// 	GENERATED_BODY()
//
// 	// 에러 코드
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly)
// 	FString ErrorCode;
//
// 	// 에러가 발생한 시스템 또는 함수
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly)
// 	FString Context;
//
// 	// 사용자에게 보여줄 에러 메시지
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly)
// 	FString Message;
//
// 	// 개발자용 상세 에러 메시지
// 	UPROPERTY(EditAnywhere, BlueprintReadOnly)
// 	FText DeveloperMessage;
// };
//
// /** 세션 작업 결과 */
// UENUM(BlueprintType)
// enum class ESessionOpResult : uint8
// {
// 	Success,
// 	Busy,
// 	NoInterface,
// 	NotFound,
// 	Full,
// 	Failed,
// };
//
// /** 방 검색 기준 */
// UENUM(BlueprintType)
// enum class ERoomSearchType : uint8
// {
// 	RoomName,	// 공개 방, 제목 부분 일치
// 	RoomCode	// 비공개 방, 코드 완전 일치
// };
//
// /** 방 정보 */
// USTRUCT(BlueprintType)
// struct FRoomListDataA
// {
// 	GENERATED_BODY()
//
// 	UPROPERTY(BlueprintReadOnly, Category = "Room")
// 	FString SessionId;
//
// 	UPROPERTY(BlueprintReadOnly, Category = "Room")
// 	FString RoomName;
//
// 	UPROPERTY(BlueprintReadOnly, Category = "Room")
// 	int32 CurrentPlayers = 0;
//
// 	UPROPERTY(BlueprintReadOnly, Category = "Room")
// 	int32 MaxPlayers = 0;
// };
//
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionOpComplete, ESessionOpResult, Result);
//
// UCLASS()
// class HEAVYHANDED_API UNetGameInstanceSubsystem : public UGameInstanceSubsystem
// {
// 	GENERATED_BODY()
//
// public:
// 	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
// 	virtual void Deinitialize() override;
//
// 	// -------------------------- 방 정보 설정 -----------------------------------
//
// 	/** 현재 참가한 방 제목 */
// 	UPROPERTY(BlueprintReadWrite, Category = "Room")
// 	FString JoinedRoomName;
//
// 	UPROPERTY(BlueprintReadWrite, Category = "Room")
// 	FString JoinedRoomCode;
//
// 	UPROPERTY(BlueprintReadWrite, Category = "Room")
// 	int32 MaxPlayers = 0;
//
// 	UFUNCTION(BlueprintPure, Category = "Room")
// 	FString GetJoinedRoomName() const;
//
// 	UFUNCTION(BlueprintPure, Category = "Room")
// 	FString GetJoinedRoomCode() const;
//
// 	UFUNCTION(BlueprintPure, Category = "Room")
// 	int32 GetRoomMaxPlayer() const;
//
//
// 	// ------------------------------ 세션 -----------------------------------
//
// 	UFUNCTION(BlueprintCallable, Category = "Session")
// 	void CreateRoom(const FString& RoomName, int32 MaxPlayer, bool bIsPublic);
//
// 	// 검색 조건 없이 공개 방 전체를 다시 받는다. 결과는 OnRoomListUpdated
// 	UFUNCTION(BlueprintCallable, Category = "Session")
// 	void RefreshRoomList();
//
// 	// 조건에 맞는 방만 RoomList 에 남긴다. 키워드가 비면 RefreshRoomList 와 같다
// 	UFUNCTION(BlueprintCallable, Category = "Session")
// 	void SearchRooms(ERoomSearchType SearchType, const FString& Keyword);
//
// 	UFUNCTION(BlueprintCallable, Category = "Session")
// 	void JoinRoomById(const FString& SessionId);
//
// 	UFUNCTION(BlueprintCallable, Category = "Session")
// 	void LeaveRoom();
//
// 	UFUNCTION(BlueprintPure, Category = "Session")
// 	bool IsBusy() const { return CurrentOp != ESessionOp::None; }
//
// 	UPROPERTY(BlueprintReadOnly, Category = "Session")
// 	TArray<FRoomListData> RoomList;
//
// 	UPROPERTY(BlueprintAssignable, Category = "Session")
// 	FOnSessionOpComplete OnCreateRoomComplete;
//
// 	UPROPERTY(BlueprintAssignable, Category = "Session")
// 	FOnSessionOpComplete OnRoomListUpdated;
//
// 	UPROPERTY(BlueprintAssignable, Category = "Session")
// 	FOnSessionOpComplete OnJoinRoomComplete;
//
// 	UPROPERTY(BlueprintAssignable, Category = "Session")
// 	FOnSessionOpComplete OnLeaveRoomComplete;
//
//
// 	// ------------------------------ 디버그용 -----------------------------------
//
// public:
//
// 	// 디버그 메시지를 저장하고 Output Log에 출력
// 	// bError가 true면 Error 로그로 출력
// 	// bError가 false면 Warning 로그로 출력
// 	void DebugInstMessage(const FString& Message, bool bError);
//
// 	// 현재까지 저장된 모든 디버그 로그 반환
// 	const TArray<FString>& GetDebugLogs() const
// 	{
// 		return DebugLogs;
// 	}
//
// 	// 저장된 디버그 로그 전체 삭제
// 	void ClearDebugLogs()
// 	{
// 		DebugLogs.Empty();
// 	}
//
//
// 	// ------------------------------ 디버그 UI -----------------------------------
//
// 	// 디버그 에러 UI로 사용할 위젯 클래스
// 	// 블루프린트에서 WBP_DebugError 같은 위젯을 지정
// 	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
// 	TSubclassOf<UUserWidget> DebugErrorWidgetClass;
//
// 	UFUNCTION(BlueprintCallable, Category = "Debug")
// 	void SetDebugErrorWidgetClass(TSubclassOf<UUserWidget> WidgetClass);
//
// 	// 저장된 디버그 로그를 UI에 표시
// 	void ShowDebugErrorUI(const TArray<FString>& Logs);
//
//
// 	// ------------------------------ 에러 정보 -----------------------------------
//
// 	// 에러 코드가 저장된 DataTable
// 	UPROPERTY(BlueprintReadOnly, Category = "Debug")
// 	UDataTable* ErrorCodeTable;
//
// 	// 에러 코드에 해당하는 정보를 DataTable에서 찾아 저장
// 	void RecordError(const FString& ErrorCode);
//
// 	// 마지막으로 기록된 에러 정보 반환
// 	const FDebugErrorInfo& GetLastError() const
// 	{
// 		return LastError;
// 	}
//
//
// private:
//
// 	// 마지막으로 발생한 에러 정보
// 	UPROPERTY()
// 	FDebugErrorInfo LastError;
//
// 	// 지금까지 저장된 디버그 로그
// 	UPROPERTY()
// 	TArray<FString> DebugLogs;
//
//
// 	// ------------------------------ 세션 내부 -----------------------------------
//
// 	enum class ESessionOp : uint8
// 	{
// 		None,
// 		Create,
// 		FindList,
// 		Join,
// 		Leave,
// 		Cleanup
// 	};
//
// 	ESessionOp CurrentOp = ESessionOp::None;
//
// 	ESessionOp PendingAfterDestroy = ESessionOp::None;
//
// 	FString PendingRoomName;
// 	int32 PendingMaxPlayer = 0;
// 	bool bPendingIsPublic = true;
//
// 	// 진행 중인 검색 조건. bSearchFiltered 가 false 면 공개 방 전체
// 	bool bSearchFiltered = false;
// 	ERoomSearchType PendingSearchType = ERoomSearchType::RoomName;
// 	FString PendingKeyword;
//
// 	// 삭제를 기다리는 동안 목록이 갱신될 수 있어 인덱스가 아니라 결과를 복사해 둔다
// 	FOnlineSessionSearchResult PendingJoinResult;
//
// 	// RoomList 와 같은 순서의 원본 검색 결과
// 	TArray<FOnlineSessionSearchResult> CachedResults;
//
// 	TSharedPtr<FOnlineSessionSearch> SessionSearch;
//
// 	FDelegateHandle CreateHandle;
// 	FDelegateHandle FindHandle;
// 	FDelegateHandle JoinHandle;
// 	FDelegateHandle DestroyHandle;
// 	FDelegateHandle NetFailureHandle;
// 	FDelegateHandle TravelFailureHandle;
//
// 	IOnlineSessionPtr GetSessionInterface() const;
// 	bool HasGameSession() const;
//
// 	void StartCreate();
// 	void StartFind();
// 	bool MatchesSearch(const FOnlineSessionSearchResult& Result) const;
// 	void BeginJoin(const FOnlineSessionSearchResult& Result);
// 	void StartJoin();
// 	void BeginDestroySession(ESessionOp After);
// 	void FinishLeave();
// 	void FinishOp(ESessionOp Op, ESessionOpResult Result);
// 	void ClearOpDelegates();
// 	void RecoverFromFailure();
//
// 	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
// 	void HandleFindComplete(bool bWasSuccessful);
// 	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
// 	void HandleDestroyComplete(FName SessionName, bool bWasSuccessful);
//
// 	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
// 	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
//
// 	FString GenerateRoomCode() const;
// 	void ClearRoomInfo();
// 	void TravelToMap(const TSoftObjectPtr<UWorld>& Map, const FString& Options);
// };
