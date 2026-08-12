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

	UPROPERTY(BlueprintAssignable, Category = "Debug")
	FOnSessionDebug OnSessionDebug;

	void SessionDebug(const FString& Message, bool bSuccess);



	bool bPendingCreateSession = false;

	FDelegateHandle DestroyHandle;

	UFUNCTION()
	void TitleOnDestroySessionComplete(FName SessionName, bool bWasSuccessful);


	
};
