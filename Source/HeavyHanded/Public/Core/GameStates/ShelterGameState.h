// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ShelterGameState.generated.h"

/**
 * 
 */

 // Delegate ¼±¾ð
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyPlayerCountChanged, int32, PlayerCount);

UCLASS()
class HEAVYHANDED_API AShelterGameState : public AGameState
{
	GENERATED_BODY()
	

public:

    UPROPERTY(BlueprintAssignable)
    FOnLobbyPlayerCountChanged OnLobbyPlayerCountChanged;

    UFUNCTION(BlueprintPure)
    int32 GetLobbyPlayerCount() const;

    void UpdateLobbyPlayerCount();

protected:

    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;


};
