// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "GameplayTagContainer.h"

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


	UFUNCTION(BlueprintPure)
	AShelterPlayerState* GetMyPlayerState() const;





	// ----------------------------------------------------------------


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


	// 채팅 UI에서 바인딩
	/** 채팅 UI 가 바인딩하는 수신 델리게이트 */

	UPROPERTY(BlueprintAssignable)
	FOnChatMessageReceived OnChatMessageReceived;

	/** 채팅 한 줄의 최대 길이. 서버 검증에 쓰인다 */
	static constexpr int32 MaxChatMessageLength = 200;

	// --------------------------------------------------

public:

	// 클라이언트에서 호출 - 서버에서 실제로 실행됨
	// Widget에서 직업 버튼을 눌렀을 때 사용
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerSelectJob(EJobType NewJob);

	// 현재 직업 선택 해제 - 역할 못바꿔서 지워도 될듯
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerClearJob();

	// 역할 확정 버튼 누르고 다음 화면 넘어갈 때
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void serverConfirmedJob();

	// -------------------------------------------------------

		// 인게임 이동
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void serverIngameTravel();


protected:

	// 직업별 Pawn 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Job")
	TMap<FGameplayTag, TSubclassOf<APawn>> JobPawnMap;

	// Pawn을 실제로 생성하고 빙의
	void SpawnJobPawn(FGameplayTag JobTag);


};
