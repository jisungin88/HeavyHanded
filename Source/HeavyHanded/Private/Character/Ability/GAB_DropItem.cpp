// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Ability/GAB_DropItem.h"
#include "Character/BaseCharacter.h" 

void UGAB_DropItem::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Commit 실패 등으로 Super 에서 이미 종료됐으면 더 진행하지 않는다.
	if (!IsActive())
	{
		return;
	}

	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();

    // 2. 서버 권한 체크 (서버에서만 물리/상태 변경)
    if (Character && Character->HasAuthority())
    {
        AActor* HeldActor = Character->GetHeldActor();
        if (HeldActor)
        {
            // 분리와 물리/콜리전 복구까지 SetHeldActor 가 처리한다.
            Character->SetHeldActor(nullptr);

            UE_LOG(LogTemp, Log, TEXT("DropItem logic moved to Ability!"));
        }
    }
	// 3. 어빌리티의 역할을 다했으므로 즉시 종료합니다.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}