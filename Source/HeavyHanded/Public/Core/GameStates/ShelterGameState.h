// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "GameplayTagContainer.h" // Tag 사용 위함
#include "Core/RunProgressView.h"
#include "Core/HeistSettings.h"
#include "Core/HeistSiteView.h"    // FHeistSiteView 를 값으로 돌려준다 — 전방 선언 불가

#include "ShelterGameState.generated.h"


// .h


 // Delegate 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyPlayerCountChanged, int32, PlayerCount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCanStartChanged, bool, bCanStart);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJobStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTravelTagChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunProgressChanged, FRunProgressView, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDepartingChanged, bool, bDeparting);


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

	UPROPERTY(BlueprintAssignable, Category = "Shelter|Run")
	FOnRunProgressChanged OnRunProgressChanged;

	UFUNCTION(BlueprintPure, Category = "Shelter|Run")
	const FRunProgressView& GetRunProgress() const { return RunProgress; }

	UFUNCTION(BlueprintCallable, Category = "Shelter|Run")
	void PublishRunProgress();

protected:

    virtual void AddPlayerState(APlayerState* PlayerState) override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;

	UPROPERTY(ReplicatedUsing = OnRep_RunProgress, BlueprintReadOnly, Category = "Shelter|Run")
	FRunProgressView RunProgress;

	UFUNCTION()
	void OnRep_RunProgress();
// --------------------------------------------------------------

public:

	UPROPERTY(BlueprintAssignable)
	FOnJobStateChanged OnJobStateChanged;

	UPROPERTY(ReplicatedUsing = OnRep_JobStateChanged, BlueprintReadOnly)
	int32 JobStateChanged = 0;

	UFUNCTION()
	void OnRep_JobStateChanged();

	UFUNCTION(BlueprintCallable, Category = "Shelter|Roster")
	void NotifyRosterDirty();


	// 해당 직업을 이미 누군가 선택했는지 검사
	UFUNCTION(BlueprintPure)
	bool IsJobAlreadySelected(EJobType Job) const;


	// 해당 직업을 선택할 수 있는지 검사
	bool CanSelectJob(EJobType Job) const;

	// 플레이어에게 직업을 실제로 적용
	bool SelectJob(AShelterPlayerState* PlayerState, EJobType NewJob);

	// 현재 직업을 해제
	bool ClearJob(AShelterPlayerState* PlayerState); //


	UPROPERTY(ReplicatedUsing = OnRep_CanStart, BlueprintReadOnly)
	bool bCanStart = false;

	UPROPERTY(BlueprintAssignable)
	FOnCanStartChanged OnCanStartChanged;

	UFUNCTION()
	void OnRep_CanStart();

	//UFUNCTION(BlueprintPure)
	bool CanStartGame() const;

	void UpdateCanStart();


	// --------------------------------------------------------------

	// 현재 접속한 모든 플레이어의 PlayerState 반환
	UFUNCTION(BlueprintPure)
	TArray<AShelterPlayerState*> GetShelterPlayerStates() const;

	UFUNCTION()
	void OnPlayerJobChanged(AShelterPlayerState* PlayerState);

	UFUNCTION(BlueprintPure, Category = "Shelter|Roster")
	TArray<FString> GetUnconfirmedPlayerNames() const;

	// ----------------------------------------------------------------

	// 현재 선택된 장소(Site)가 변경되었을 때 UI에 알림
	UPROPERTY(BlueprintAssignable)
	FOnTravelTagChanged OnTravelTagChanged;

	/** 선택한 진입점 */
	UPROPERTY(ReplicatedUsing = OnRep_SelectedEntry, BlueprintReadOnly, Category = "Shelter|Travel")
	FGameplayTag SelectedEntry;

	void SetSelectedEntry(FGameplayTag NewEntry);

	/** 진입점 리스트 */
	UFUNCTION(BlueprintPure, Category = "Shelter|Travel")
	TArray<FHeistEntryOption> GetEntryOptions() const;

	void EnsureEntrySelected();

	UFUNCTION(BlueprintPure, Category = "Shelter|Travel")
	FGameplayTag GetNextEntryOption() const;

	UFUNCTION(BlueprintPure, Category = "Shelter|Travel")
	bool FindEntryOption(FGameplayTag EntryTag, FHeistEntryOption& OutOption) const;

	/**
	 * 이 장소를 화면에 그리는 데 필요한 것 전부. 등록되지 않은 장소면 이름만 채운 빈 뷰다.
	 * 표(DT_SiteCatalog)와 진행 상황을 여기서 합쳐 주므로 위젯은 어디서 왔는지 몰라도 된다.
	 */
	UFUNCTION(BlueprintPure, Category = "Shelter|Travel")
	FHeistSiteView GetSiteView(FGameplayTag SiteTag) const;

	/** 지금 갈 장소의 표시 정보. 출발 문 위젯이 이것 하나만 쓴다 */
	UFUNCTION(BlueprintPure, Category = "Shelter|Travel")
	FHeistSiteView GetNextSiteView() const;

	// ---- 출발
	UPROPERTY(ReplicatedUsing = OnRep_bDeparting, BlueprintReadOnly, Category = "Shelter|Travel")
	bool bDeparting = false;

	UPROPERTY(BlueprintAssignable, Category = "Shelter|Travel")
	FOnDepartingChanged OnDepartingChanged;

	void SetDeparting(bool bNewDeparting);

	UFUNCTION()
	void OnRep_bDeparting();

	UFUNCTION()
	void OnRep_SelectedEntry();

	// 모든 태그 디버그
	UFUNCTION(BlueprintCallable)
	void DebugPrintGameplayTags();


	// Replication에 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//-----------------------닉네임
public:
	static constexpr int32 MinNicknameLength = 2;
	static constexpr int32 MaxNicknameLength = 12;

	UFUNCTION(BlueprintPure, Category = "Shelter|Nickname")
	static FString SanitizeNickname(const FString& Raw);

	UFUNCTION(BlueprintPure, Category = "Shelter|Nickname")
	static ENicknameError ValidateNicknameFormat(const FString& Clean);

	UFUNCTION(BlueprintPure, Category = "Shelter|Nickname")
	bool IsNicknameTaken(const FString& Clean, const APlayerState* Exclude) const;
};
