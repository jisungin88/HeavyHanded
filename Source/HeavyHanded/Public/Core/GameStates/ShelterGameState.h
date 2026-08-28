// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "GameplayTagContainer.h" // Tag 사용 위함

#include "ShelterGameState.generated.h"

/**
 * 
 */


UENUM(BlueprintType)
enum class EEntryTag : uint8
{
	None,
	Front,
	Garage,
	Alley
};

UENUM(BlueprintType)
enum class ESiteTag : uint8
{
	None,
	Mansion,
	Museum,
	Bank
};




 // Delegate 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyPlayerCountChanged, int32, PlayerCount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJobStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTravelTagChanged);

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



	// 해당 직업을 이미 누군가 선택했는지 검사
	UFUNCTION(BlueprintPure)
	bool IsJobAlreadySelected(EJobType Job) const;


	// 해당 직업을 선택할 수 있는지 검사
	bool CanSelectJob(EJobType Job) const;

	// 플레이어에게 직업을 실제로 적용
	bool SelectJob(AShelterPlayerState* PlayerState, EJobType NewJob);

	// 현재 직업을 해제
	bool ClearJob(AShelterPlayerState* PlayerState);

	// --------------------------------------------------------------

	// 현재 접속한 모든 플레이어의 PlayerState 반환
	UFUNCTION(BlueprintPure)
	TArray<AShelterPlayerState*> GetShelterPlayerStates() const;

	UFUNCTION()
	void OnPlayerJobChanged(AShelterPlayerState* PlayerState);

	// ----------------------------------------------------------------

	// 현재 선택된 장소(Site)가 변경되었을 때 UI에 알림
	UPROPERTY(BlueprintAssignable)
	FOnTravelTagChanged OnTravelTagChanged;

	// 현재 선택된 Entry
	UPROPERTY(ReplicatedUsing = OnRep_EntryTag, BlueprintReadOnly)
	EEntryTag EntryTag = EEntryTag::None;

	// 현재 선택된 Site
	UPROPERTY(ReplicatedUsing = OnRep_SiteTag, BlueprintReadOnly)
	ESiteTag SiteTag = ESiteTag::None;

	// 서버에서 Entry를 변경
	// PlayerController의 Server RPC에서 호출
	UFUNCTION(BlueprintCallable)
	void SetEntryTag(EEntryTag NewTag);

	// 서버에서 Site를 변경
	// PlayerController의 Server RPC에서 호출
	UFUNCTION(BlueprintCallable)
	void SetSiteTag(ESiteTag NewTag);


	// SiteTag가 클라이언트에 복제되었을 때 호출
	UFUNCTION()
	void OnRep_SiteTag();

	// EntryTag가 클라이언트에 복제되었을 때 호출
	UFUNCTION()
	void OnRep_EntryTag();

	// Replication에 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

};
