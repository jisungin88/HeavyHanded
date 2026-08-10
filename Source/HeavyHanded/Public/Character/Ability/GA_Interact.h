// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Interact.generated.h"

UCLASS()
class HEAVYHANDED_API UGA_Interact : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Interact();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // 에디터에서 지정할 상호작용 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Animation")
    TObjectPtr<UAnimMontage> SkillMontage;

    // 상호작용 레이캐스트 및 로직 수행 함수
    void PerformInteraction();

    // 몽타주가 끝났을 때 호출될 콜백 함수
    UFUNCTION()
    void OnMontageFinished();
};
