// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShelterPlayerController.generated.h"

/**
 * 
 */
class AMyGameState;

UCLASS()
class HEAVYHANDED_API AShelterPlayerController : public APlayerController
{
	GENERATED_BODY()
	

public:

    UFUNCTION(BlueprintImplementableEvent)
    void BP_OnPlayerListChanged();

protected:

    virtual void BeginPlay() override;

    UFUNCTION()
    void HandlePlayerListChanged();


};
