// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseGameplayAbility.h"
#include "GA_DropItem.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UGA_DropItem : public UBaseGameplayAbility
{
	GENERATED_BODY()
public:
	// 어빌리티가 실행될 때 호출되는 함수
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;
};
