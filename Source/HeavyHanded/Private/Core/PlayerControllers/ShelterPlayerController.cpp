// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerControllers/ShelterPlayerController.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Core/PlayerStates/ShelterPlayerState.h"

void AShelterPlayerController::BeginPlay()
{
    Super::BeginPlay();

}

void AShelterPlayerController::ClientReceiveChatMessage_Implementation(const FString& PlayerName, const FString& Message)
{
	OnChatMessageReceived.Broadcast(PlayerName, Message);
}

void AShelterPlayerController::ServerSendChatMessage_Implementation(const FString& Message)
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
			PC->ClientReceiveChatMessage(PlayerName, Message);
		}
	}

}







