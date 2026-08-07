// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "TitlePlayerController.generated.h"


//class IOnlineSession;
class FOnlineSessionSearch;

USTRUCT(BlueprintType)
struct FRoomListData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RoomName;

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentPlayers;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxPlayers;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionCreated, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomListUpdated, bool, bSuccess);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomListUpdated);

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

    // ------------------------------------------------

    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnSessionCreated OnSessionCreated;

    UPROPERTY(BlueprintAssignable, Category = "Session")
    FOnRoomListUpdated OnRoomListUpdated;

    // -------------------------------------------------

    virtual void BeginPlay() override;
    
    // -------------------------------------------------

    UFUNCTION(BlueprintCallable)
    void TitleCreateSession
    (const FString& RoomName, int maxPlayer, bool isPublic);

    UFUNCTION(BlueprintCallable)
    void TitleFindSessions();

    UFUNCTION(BlueprintCallable)
    void TitleJoinSession(int32 SearchIndex);


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


    //厚傍俺 规 内靛 积己
    FString GenerateRoomCode();
	
};
