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
    // 시선 기준 상호작용 사거리. 눈 위치에서 바라보는 방향으로 이만큼 훑는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
    float InteractionRange = 300.f;

    // 스윕 구체 반경. 크면 조준이 관대해지지만 엉뚱한 대상이 걸리기 쉬워진다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
    float InteractionRadius = 30.f;

    // 상호작용 레이캐스트 및 로직 수행 함수
    void PerformInteraction();

    // 몽타주가 끝났을 때 호출될 콜백 함수
    UFUNCTION()
    void OnMontageFinished();
};
