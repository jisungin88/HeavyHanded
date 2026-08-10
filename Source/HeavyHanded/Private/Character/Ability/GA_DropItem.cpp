// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Ability/GA_DropItem.h"
#include "BaseCharacter.h" 

void UGA_DropItem::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 1. 현재 이 어빌리티를 사용한 캐릭터를 ABaseCharacter로 캐스팅합니다.
	if (ABaseCharacter* Character = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()))
	{
		// 2. 캐릭터에 구현해 둔 DropItem 함수를 호출합니다. 
		// (이 함수 안에 이미 서버 RPC와 멀티캐스트 동기화 처리가 들어있으므로 알아서 멀티가 됩니다!)
		Character->DropItem();
	}

	// 3. 어빌리티의 역할을 다했으므로 즉시 종료합니다.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}