// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseGameplayAbility.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GAB_Throw.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UGAB_Throw : public UBaseGameplayAbility
{
	GENERATED_BODY()

public:
	UGAB_Throw();

	// 던지기는 "일반 노획물을 들고 있을 때"만 성립한다.
	// 몽타주가 뜨기 전(커밋·태스크 생성 이전)에 막아야 한다 — ActivateAbility 안에서
	// 늦게 걸러내면, 부모의 PlayMontageAndWait 이 bStopWhenAbilityEnds=false 로 떠 있어서
	// EndAbility 를 불러도 이미 재생 중인 몽타주가 멈추지 않는다(BaseGameplayAbility.cpp).
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	void UpdateTrajectoryPreview();
	void ClearTrajectoryPreview();

	// 조준 hold 루프 제어: HoldStart 구간에서 정지(자기 루프)해두었다가, 손을 떼면 HoldEnd(던지는 모션)로 흘려보낸다.
	void BeginHoldLoop();
	void ReleaseHoldLoop();

	// SkillMontage 의 HoldEnd 구간 안, 물체가 실제로 손을 떠나는 프레임에 찍어둔 Notify 콜백.
	UFUNCTION()
	void OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	// 실제 던지기(물리 임펄스 / ICarryable 위임) 실행.
	void ExecuteThrow();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	float ThrowSpeed = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	float ProjectileRadius = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	float TrajectoryUpdateInterval = 0.03f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	FVector MuzzleOffset = FVector(0.f, 0.f, 50.f);

	// 조준 유지 중 정지 루프를 도는 구간 이름 (SkillMontage 안에 존재해야 함).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	FName HoldStartSectionName = TEXT("HoldStart");

	// 손을 뗐을 때 진입해서 몽타주 끝까지 이어지는 던지는 모션 구간 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	FName HoldEndSectionName = TEXT("HoldEnd");

	// HoldEnd 구간 안, 물체가 손을 떠나는 프레임에 찍어둘 Play Montage Notify 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Throw")
	FName ThrowReleaseNotifyName = TEXT("Throw_Release");

private:
	FTimerHandle TrajectoryUpdateTimerHandle;
	bool bThrowExecuted = false;
};
