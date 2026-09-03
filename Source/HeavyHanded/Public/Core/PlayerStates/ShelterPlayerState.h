// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
//#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "Character/PlayerSessionState.h"
//#include "AbilitySystemInterface.h"

#include "ShelterPlayerState.generated.h"

/**
 *
 */

 // 플레이어가 선택할 수 있는 직업
 // 별도의 JobType.h를 만들지 않고 PlayerState.h에서 관리

UENUM(BlueprintType)
enum class EJobType : uint8
{
	None    UMETA(DisplayName = "None"),

	Brute   UMETA(DisplayName = "Brute"),   // 힘
	Ghost   UMETA(DisplayName = "Ghost"),   // 이동
	Oracle    UMETA(DisplayName = "Oracle"),
	Mimic    UMETA(DisplayName = "Mimic")
};

/** Role.* 태그 → EJobType. 매칭 없으면 None */
HEAVYHANDED_API EJobType JobTypeFromRoleTag(const FGameplayTag& RoleTag);
/** EJobType → Role.* 태그. None 이면 무효 태그 */
HEAVYHANDED_API FGameplayTag RoleTagFromJobType(EJobType Job);

/// DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectedJobChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedJobChanged, AShelterPlayerState*, PlayerState);


class AShelterGameState;

UCLASS()
class HEAVYHANDED_API AShelterPlayerState : public APlayerSessionState
{
	GENERATED_BODY()


public:


	// 현재 플레이어가 선택한 직업
	// 서버에서 변경하면 클라이언트들에게 자동으로 복제됨
	UPROPERTY(ReplicatedUsing = OnRep_SelectedJob, BlueprintReadOnly)
	EJobType SelectedJob = EJobType::None;


protected:

	// 클라이언트에서 SelectedJob이 변경됐을 때 호출
	UFUNCTION()
	void OnRep_SelectedJob();


public:

	UPROPERTY(BlueprintAssignable)
	FOnSelectedJobChanged OnSelectedJobChanged;

	// 현재 플레이어의 직업을 변경
	// 서버에서만 호출
	UFUNCTION(BlueprintCallable)
	void SetSelectedJob(EJobType NewJob);


	// 현재 선택된 직업 반환
	UFUNCTION(BlueprintPure)
	EJobType GetSelectedJob() const { return SelectedJob; }


	// Replication 등록
	virtual void GetLifetimeReplicatedProps
		(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// // ---------------------------- 상호작용 테스트용 코드 : 해결되면 상속 형태로 반드시 수정할 것

	// -----------------------------------------------------------------------------------------------


};

