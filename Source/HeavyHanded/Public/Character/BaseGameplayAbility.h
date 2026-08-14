// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "HAL/IConsoleManager.h"
#include "BaseGameplayAbility.generated.h"

#if ENABLE_DRAW_DEBUG
// [디버그 전용] 어빌리티 시각화 스위치. 정의는 BaseGameplayAbility.cpp 참조.
extern TAutoConsoleVariable<int32> CVarAbilityDebug;
#endif

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UBaseGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	// 에디터(블루프린트)에서 스킬마다 다른 몽타주를 지정할 수 있도록 오픈합니다.
	// 비워두면 몽타주 없이 실행된다 — 그 경우 어빌리티를 언제 끝낼지는 파생 클래스 몫이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> SkillMontage;

	// 이번 활성화에서 몽타주 재생 태스크를 걸었는지.
	// false 면 어빌리티를 끝내줄 몽타주 콜백이 없으므로 파생 클래스가 직접 EndAbility 해야 한다.
	UFUNCTION(BlueprintPure, Category = "GAS|Ability")
	bool IsPlayingSkillMontage() const { return bSkillMontageStarted; }

	UFUNCTION(BlueprintCallable, Category = "GAS|Ability")
	ABaseCharacter* GetBaseCharacterFromActorInfo() const;

protected:
	// 몽타주 관련 콜백 함수들 (UFUNCTION 필수)
	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageBlendOut();

	UFUNCTION()
	virtual void OnMontageInterrupted();

	UFUNCTION()
	virtual void OnMontageCancelled();
	// ... 기존 코드

protected:
	// 클라이언트에서 서버 ASC로 Gameplay Event 태그를 전송하는 공통 함수
	UFUNCTION(BlueprintCallable, Category = "GAS|Ability")
	void SendGameplayEventToASCOnServer(FGameplayTag EventTag, const FGameplayEventData& Payload);

public:
	UBaseGameplayAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool bSkillMontageStarted = false;

	// ReadyForActivation() 실행 중인지. 그 안에서 곧바로 취소가 오면 몽타주가
	// 아예 재생되지 못한 것이므로 어빌리티를 끝내지 않고 "몽타주 없음"으로 강등한다.
	bool bMontageActivationInProgress = false;
};
