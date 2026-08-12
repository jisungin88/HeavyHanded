// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "BaseGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UBaseGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	// 에디터(블루프린트)에서 스킬마다 다른 몽타주를 지정할 수 있도록 오픈합니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> SkillMontage;

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
};
