// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "TitlePlayerController.generated.h"


//class IOnlineSession;
class FOnlineSessionSearch;

USTRUCT(BlueprintType)
struct FRoomListData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RoomName="";

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentPlayers=0;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxPlayers=0;


    FOnlineSessionSearchResult SessionResult;

};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionCreated, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomListUpdated, bool, bSuccess);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomListUpdated);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams
(FOnSessionDebug, FString, DebugMessage, bool, bSuccess);

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API ATitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

    UPROPERTY(BlueprintReadOnly)
    TArray<FRoomListData> RoomList;

    TArray<FOnlineSessionSearchResult> PublicSessionResults;

    // ------------------------------------------------

    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnSessionCreated OnSessionCreated;

    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnRoomListUpdated OnRoomListUpdated;

    // -------------------------------------------------

    virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // -------------------------------------------------

    UFUNCTION(BlueprintCallable)
    void TitleCreateSession
    (const FString& RoomName, int maxPlayer, bool isPublic);

    UFUNCTION(BlueprintCallable)
    void TitleFindSessions();

    UFUNCTION(BlueprintCallable)
    void TitleJoinSession(int32 SearchIndex);
    //void TitleJoinSession(int32 SearchIndex, const FOnlineSessionSearchResult& Result);
    //void TitleJoinSession(const FOnlineSessionSearchResult& Result)


private:

    IOnlineSessionPtr SessionInterface;
    
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    

    void TitleOnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    
    void TitleOnStartSessionComplete(FName SessionName, bool bWasSuccessful);

    void TitleOnFindSessionsComplete(bool bWasSuccessful);
    
    void TitleOnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    

    FDelegateHandle CreateHandle;
    FDelegateHandle StartHandle;

    FDelegateHandle FindHandle;

    FDelegateHandle JoinHandle;


    //비공개 방 코드 생성
    FString GenerateRoomCode();





	/**
	 * DestroySession 이 끝난 뒤 이어서 할 일.
	 *
	 * [왜 필요한가] 기존 GameSession 이 남아 있으면 생성도 참가도 **곧바로 할 수 없다.**
	 *   OnlineSubsystemNull 이 "Session (GameSession) already exists, can't join twice" 로
	 *   거절하고, 그 결과는 Join Result = 5(UnknownError)로만 돌아온다.
	 *   그래서 먼저 지우고, 완료 콜백에서 원래 하려던 것을 이어서 한다.
	 *
	 * 한 번에 하나만 예약된다 — 두 경로 모두 예약 직후 곧바로 반환하기 때문이다.
	 */
	enum class EPendingSessionAction : uint8
	{
		None,
		Create,
		Join
	};

	EPendingSessionAction PendingAction = EPendingSessionAction::None;

	/** PendingAction == Create 일 때 다시 넘길 인자 */
	FString PendingRoomName;
	int32 PendingMaxPlayer = 0;
	bool bPendingIsPublic = true;

	/** PendingAction == Join 일 때 다시 쓸 PublicSessionResults 인덱스 */
	int32 PendingJoinIndex = INDEX_NONE;

	/**
	 * 기존 세션을 지우고 PendingAction 을 예약한다.
	 * @return 삭제를 시작했으면 true — 호출부는 곧바로 반환해야 한다
	 */
	bool BeginDestroyForPendingAction();

	FDelegateHandle DestroyHandle;

	UFUNCTION()
	void TitleOnDestroySessionComplete(FName SessionName, bool bWasSuccessful);


	UPROPERTY(BlueprintAssignable, Category = "Debug")
	FOnSessionDebug OnSessionDebug;

	void SessionDebug(const FString& Message, bool bSuccess);

	void JoinDebug(int32 SearchIndex);

};
