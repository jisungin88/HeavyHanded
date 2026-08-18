// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NetGameInstanceSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UNetGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()


public:
	// 현재 참가한 방 제목
	UPROPERTY(BlueprintReadWrite)
	FString JoinedRoomName;

	UFUNCTION(BlueprintPure, Category = "Room")
	FString GetJoinedRoomName() const;


};
