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

UENUM(BlueprintType)
enum class ENicknameError : uint8
{
	None				UMETA(DisplayName = "없음"),
	Empty				UMETA(DisplayName = "빈 값"),
	TooShort			UMETA(DisplayName = "너무 짧음"),
	TooLong				UMETA(DisplayName = "너무 김"),
	InvalidCharacters	UMETA(DisplayName = "허용되지 않은 문자 포함"),
	ContainsProfanity	UMETA(DisplayName = "금칙어 포함"),
	Taken				UMETA(DisplayName = "이미 사용 중인 닉네임"),
	NetworkError		UMETA(DisplayName = "서버 통신 오류"),
	Unknown				UMETA(DisplayName = "알 수 없는 오류")
};

USTRUCT(BlueprintType)
struct FNicknameFeedback
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shelter|Nickname")
	ENicknameError Error = ENicknameError::None;

	UPROPERTY(BlueprintReadOnly, Category = "Shelter|Nickname")
	uint8 Seq = 0;
};


/** Role.* 태그 → EJobType. 매칭 없으면 None */
HEAVYHANDED_API EJobType JobTypeFromRoleTag(const FGameplayTag& RoleTag);
/** EJobType → Role.* 태그. None 이면 무효 태그 */
HEAVYHANDED_API FGameplayTag RoleTagFromJobType(EJobType Job);

/// DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectedJobChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedJobChanged, AShelterPlayerState*, PlayerState);

/** 역할 확정 상태가 바뀌었을 때. UI 페이지 전환은 이쪽을 구독한다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJobConfirmedChanged, AShelterPlayerState*, PlayerState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNicknameRejected, ENicknameError, Error);

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


	// 역할을 "확정"했는지. SelectedJob 은 버튼을 누른 순간 이미 채워지므로
	// "고르는 중"과 "확정"을 한 변수로 겸할 수 없다.
	// 서버가 serverConfirmedJob 의 가드를 전부 통과했을 때만 true 가 된다
	UPROPERTY(ReplicatedUsing = OnRep_JobConfirmed, BlueprintReadOnly)
	bool bJobConfirmed = false;


protected:

	virtual void OnRep_PlayerName() override;

	// 클라이언트에서 SelectedJob이 변경됐을 때 호출
	UFUNCTION()
	void OnRep_SelectedJob();

	// 클라이언트에서 bJobConfirmed가 변경됐을 때 호출
	UFUNCTION()
	void OnRep_JobConfirmed();


public:

	UPROPERTY(BlueprintAssignable)
	FOnSelectedJobChanged OnSelectedJobChanged;

	UPROPERTY(BlueprintAssignable)
	FOnJobConfirmedChanged OnJobConfirmedChanged;

	// 현재 플레이어의 직업을 변경
	// 서버에서만 호출
	UFUNCTION(BlueprintCallable)
	void SetSelectedJob(EJobType NewJob);


	// 현재 선택된 직업 반환
	UFUNCTION(BlueprintPure)
	EJobType GetSelectedJob() const { return SelectedJob; }


	// 역할 확정 상태를 변경
	// 서버에서만 호출
	UFUNCTION(BlueprintCallable)
	void SetJobConfirmed(bool bNewConfirmed);


	// 역할을 확정했는지 반환. WBP_ShelterParty 의 ApplyJobPage 가 이 값만 본다
	UFUNCTION(BlueprintPure)
	bool IsJobConfirmed() const { return bJobConfirmed; }


	// Replication 등록
	virtual void GetLifetimeReplicatedProps
		(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	// // ---------------------------- 상호작용 테스트용 코드 : 해결되면 상속 형태로 반드시 수정할 것

	// -----------------------------------------------------------------------------------------------

	// --------------------- 닉네임
public:
	/** 닉네임 거절 */
	UPROPERTY(ReplicatedUsing = OnRep_NicknameFeedback, BlueprintReadOnly, Category = "Shelter|Nickname")
	FNicknameFeedback NicknameFeedback;

	UPROPERTY(BlueprintAssignable, Category = "Shelter|Nickname")
	FOnNicknameRejected OnNicknameRejected;

	void ReportNicknameError(ENicknameError Error);

protected:
	UFUNCTION()
	void OnRep_NicknameFeedback();

};

