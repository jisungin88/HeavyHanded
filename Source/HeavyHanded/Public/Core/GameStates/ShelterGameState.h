// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/PlayerStates/ShelterPlayerState.h"

#include "ShelterGameState.generated.h"

/**
 * 
 */

 // Delegate 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyPlayerCountChanged, int32, PlayerCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJobStateChanged);

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


// --------------------------------------------------------------

public:

	UPROPERTY(BlueprintAssignable)
	FOnJobStateChanged OnJobStateChanged;

	UPROPERTY(ReplicatedUsing = OnRep_JobStateChanged, BlueprintReadOnly)
	int32 JobStateChanged = 0;

	UFUNCTION()
	void OnRep_JobStateChanged();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// 해당 직업을 이미 누군가 선택했는지 검사
	UFUNCTION(BlueprintPure)
	bool IsJobAlreadySelected(EJobType Job) const;


	// 해당 직업을 선택할 수 있는지 검사
	bool CanSelectJob(EJobType Job) const;

	// 플레이어에게 직업을 실제로 적용
	bool SelectJob(AShelterPlayerState* PlayerState, EJobType NewJob);

	// 현재 직업을 해제
	bool ClearJob(AShelterPlayerState* PlayerState);


};
