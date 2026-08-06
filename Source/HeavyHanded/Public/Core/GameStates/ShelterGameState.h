// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ShelterGameState.generated.h"

/**
 * 
 */

 // Delegate ¼±¾ð
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerListChanged);

UCLASS()
class HEAVYHANDED_API AShelterGameState : public AGameState
{
	GENERATED_BODY()
	

public:

    UPROPERTY(BlueprintAssignable)
    FOnPlayerListChanged OnPlayerListChanged;

    void BroadcastPlayerListChanged();


};
