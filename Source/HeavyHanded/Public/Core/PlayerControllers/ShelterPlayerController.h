// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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

	/**
	 * 채팅 한 줄을 서버로 보낸다. 채팅 UI(WBP_ChatBox)가 호출한다.
	 *
	 * WithValidation 은 문서 02 네트워크의 규칙이자 실제 방어선이다 —
	 * 클라이언트는 임의 길이의 문자열을 보낼 수 있고, 서버는 그것을 전원에게 되뿌린다.
	 * 검증이 없으면 한 명이 거대한 문자열 하나로 전원의 대역폭을 먹을 수 있다.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable)
	void Server_SendChatMessage(const FString& Message);

	/** 서버가 해당 클라이언트에게 채팅 한 줄을 전달한다 */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveChatMessage(
		const FString& PlayerName,
		const FString& Message
	);

	/** 채팅 UI 가 바인딩하는 수신 델리게이트 */
	UPROPERTY(BlueprintAssignable)
	FOnChatMessageReceived OnChatMessageReceived;

	/** 채팅 한 줄의 최대 길이. 서버 검증에 쓰인다 */
	static constexpr int32 MaxChatMessageLength = 200;

};
