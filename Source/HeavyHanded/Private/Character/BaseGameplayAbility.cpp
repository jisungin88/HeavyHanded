// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BaseGameplayAbility.h"
#include "GameFramework/Character.h"
#include "Character/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

UBaseGameplayAbility::UBaseGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UBaseGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (SkillMontage)
    {
        ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
        if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
        {
            // 몽타주를 재생하고 재생된 길이를 가져올 수도 있습니다.
            Character->PlayAnimMontage(SkillMontage);
        }
    }
}

void UBaseGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBaseGameplayAbility::SendGameplayEventToASCOnServer(FGameplayTag EventTag, const FGameplayEventData& Payload)
{
    const FGameplayAbilityActorInfo& ActorInfo = GetActorInfo();

    if (ActorInfo.AvatarActor.IsValid())
    {
        // GAS 블루프린트 라이브러리가 제공하는 표준 이벤트 전송 함수 사용 (내부적으로 네트워킹 자동 처리)
        UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
            ActorInfo.AvatarActor.Get(),
            EventTag,
            Payload
        );
    }
}

//삭제 예정
ABaseCharacter* UBaseGameplayAbility::GetBaseCharacterFromActorInfo() const
{
    if (!CurrentActorInfo) return nullptr;
    return Cast<ABaseCharacter>(CurrentActorInfo->AvatarActor.Get());
}
