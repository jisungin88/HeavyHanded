#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Character/Ability/AT_MonitorHeavyCarry.h"
#include "GA_HeavyCarryAssist.generated.h"

class ABaseCharacter;

// 무거운 물건에 두 번째로 E 를 눌렀을 때 GAB_Interact 가 이벤트로 발동시키는 어빌리티.
// 입력으로 직접 누를 수 없다 (AbilityInputBindings 에 등록하지 않는다).
UCLASS()
class HEAVYHANDED_API UGA_HeavyCarryAssist : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HeavyCarryAssist();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 정상 종료(거리초과/넉백/주 운반자 release)와 외부 강제 취소(자발적 해제, 사망, 접속 종료)
	// 모두 이 한 곳으로 수렴시켜 MonitorTask 종료와 CleanupCrossReferences 가
	// 어떤 경로에서도 정확히 한 번만 실행되게 한다.
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// 순수 상태 태그(State.Carrying.Coop)만 부여하는 GE. 스탯 모디파이어는 없다 —
	// 실제 이동속도 보정은 SoloHeavyCarryPenaltyEffectClass 제거만으로 이미 충족된다.
	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry")
	TSubclassOf<UGameplayEffect> CoopCarryStateEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry")
	float MaxAssistDistance = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry|Tug")
	float TugOpposingDotThreshold = -0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry|Tug")
	float TugTensionBuildRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry|Tug")
	float TugTensionDecayRate = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry|Tug")
	float TugTensionKnockbackThreshold = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry|Knockback")
	float KnockbackLaunchStrength = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry|Knockback")
	TObjectPtr<UAnimMontage> KnockbackStaggerMontage;

	// 스태거 동안 이동을 막는 GE. BaseAttributeSet 의 MovementSpeed 를 0으로 만들어서,
	// 이미 있는 OnMovementSpeedChanged -> MaxWalkSpeed 파이프라인을 그대로 재사용한다.
	// (별도 bIsStaggered bool 이나 입력 차단 코드가 필요 없다)
	UPROPERTY(EditDefaultsOnly, Category = "HeavyCarry|Knockback")
	TSubclassOf<UGameplayEffect> StaggeredMovementLockEffectClass;

private:
	UPROPERTY()
	TObjectPtr<ABaseCharacter> Assistant;

	UPROPERTY()
	TObjectPtr<ABaseCharacter> PrimaryCarrier;

	UPROPERTY()
	TObjectPtr<AActor> CarriedItem;

	// EndAbility 에서 강제 종료시켜야 하므로 로컬 변수가 아니라 멤버로 들고 있는다.
	UPROPERTY()
	TObjectPtr<class UAT_MonitorHeavyCarry> MonitorTask;

	UFUNCTION()
	void HandleAssistEnded(EHeavyCarryEndReason Reason);

	// 주 운반자가 SetHeldActor(nullptr) 등으로 물건을 놓는 순간 BaseCharacter가 보내는
	// Event.Loot.Dropped 를 즉시 받아 MonitorTask 를 끝낸다 (폴링 한 틱 지연 제거).
	UFUNCTION()
	void HandleImmediatePrimaryRelease(FGameplayEventData Payload);

	UFUNCTION()
	void OnStaggerMontageFinished();

	// CleanupCrossReferences 가 여러 종료 경로(HandleAssistEnded, 외부 강제 취소)에서
	// 중복 실행되지 않도록 막는 가드. 중복 실행되면 솔로 페널티 GE가 이중 적용될 수 있다.
	bool bCrossReferencesCleaned = false;

	void CleanupCrossReferences();
};
