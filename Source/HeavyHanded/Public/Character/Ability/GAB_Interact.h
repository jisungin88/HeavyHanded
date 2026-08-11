// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Character/BaseGameplayAbility.h"
#include "GAB_Interact.generated.h"

UCLASS()
class HEAVYHANDED_API UGAB_Interact : public UBaseGameplayAbility
{
    GENERATED_BODY()

public:
    UGAB_Interact();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // 상호작용 레이캐스트 및 로직 수행 함수
    void PerformInteraction();

    // 몽타주가 끝났을 때 호출될 콜백 함수
    UFUNCTION()
    void OnMontageFinished();
};
