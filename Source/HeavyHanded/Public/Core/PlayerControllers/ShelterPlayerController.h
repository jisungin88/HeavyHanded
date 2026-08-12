// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"

#include "ShelterPlayerController.generated.h"


/**
 * 
 */


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnChatMessageReceived,
	const FString&, PlayerName,
	const FString&, Message
);


UCLASS()
class HEAVYHANDED_API AShelterPlayerController : public APlayerController
{
	GENERATED_BODY()
	

protected:

    virtual void BeginPlay() override;


public:


	// 클라이언트 → 서버
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerSendChatMessage(const FString& Message);

	// 서버 → 해당 클라이언트
	UFUNCTION(Client, Reliable)
	void ClientReceiveChatMessage(
		const FString& PlayerName,
		const FString& Message
	);

	// 채팅 UI에서 바인딩
	UPROPERTY(BlueprintAssignable)
	FOnChatMessageReceived OnChatMessageReceived;


};
