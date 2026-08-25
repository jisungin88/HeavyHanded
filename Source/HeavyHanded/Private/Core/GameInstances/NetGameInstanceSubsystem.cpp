// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameInstances/NetGameInstanceSubsystem.h"

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
