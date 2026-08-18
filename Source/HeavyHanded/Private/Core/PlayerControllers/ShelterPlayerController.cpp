// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerControllers/ShelterPlayerController.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Core/GameInstances/NetGameInstanceSubsystem.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"



void AShelterPlayerController::BeginPlay()
{
    Super::BeginPlay();

	UNetGameInstanceSubsystem* NetSubsystem =
		GetGameInstance()->GetSubsystem<UNetGameInstanceSubsystem>();

	if (!NetSubsystem)
	{
		return;
	}

	const FString& RoomName =
		NetSubsystem->JoinedRoomName;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Lobby Room Name = %s"),
		*RoomName
	);

}

//void AShelterPlayerController::Client_ReceiveChatMessage(const FString& PlayerName, const FString& Message)
//{
//}



void AShelterPlayerController::Client_ReceiveChatMessage_Implementation(const FString& PlayerName, const FString& Message)
{
	OnChatMessageReceived.Broadcast(PlayerName, Message);
}

bool AShelterPlayerController::Server_SendChatMessage_Validate(const FString& Message)
{
	// 검증에 실패하면 엔진이 해당 클라이언트의 접속을 끊는다.
	// 그래서 "빈 문자열" 같은 정상 범위의 잘못된 입력은 여기서 막지 않는다 —
	// 그건 아래 _Implementation 에서 조용히 무시한다.
	// 여기서 막는 것은 악의적인 입력뿐이다.
	return Message.Len() <= MaxChatMessageLength;
}

void AShelterPlayerController::Server_SendChatMessage_Implementation(const FString& Message)
{
	if (Message.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	int32 PlayerIndex = 0;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AShelterPlayerController* PC =
			Cast<AShelterPlayerController>(It->Get());

		if (PC)
		{
			if (PC == this)
			{
				break;
			}

			++PlayerIndex;
		}
	}

	FString PlayerName = FString::Printf(
		TEXT("Player_%d"),
		PlayerIndex
	);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AShelterPlayerController* PC =
			Cast<AShelterPlayerController>(It->Get());

		if (PC)
		{
			PC->Client_ReceiveChatMessage(PlayerName, Message);
		}
	}

}







