// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MyGameInstance.generated.h"

/**
 * 
 */


USTRUCT(BlueprintType)
struct FRoomInfoData // 가능한 타이틀 pc와 합칠 것
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString RoomName;

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentPlayers;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxPlayers;

};



UCLASS()
class HEAVYHANDED_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
    FRoomInfoData RoomData;

};
