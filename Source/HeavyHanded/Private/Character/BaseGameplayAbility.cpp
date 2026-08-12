// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BaseGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
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

    // 2. 태스크 생성
    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        SkillMontage,
        1.0f,
        NAME_None,
        false);

    // 3. 델리게이트 바인딩
    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UBaseGameplayAbility::OnMontageCompleted);
        MontageTask->OnBlendOut.AddDynamic(this, &UBaseGameplayAbility::OnMontageBlendOut);
        MontageTask->OnInterrupted.AddDynamic(this, &UBaseGameplayAbility::OnMontageInterrupted);
        MontageTask->OnCancelled.AddDynamic(this, &UBaseGameplayAbility::OnMontageCancelled);

        // 4. 중요: 태스크 시작
        MontageTask->ReadyForActivation();
    }

    //if (SkillMontage)
    //{
    //    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    //    if (Character && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
    //    {
    //        // 몽타주를 재생하고 재생된 길이를 가져올 수도 있습니다.
    //        Character->PlayAnimMontage(SkillMontage);
    //    }
    //}
}

// 몽타주가 끝났을 때 어빌리티 종료
void UBaseGameplayAbility::OnMontageCompleted()
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UBaseGameplayAbility::OnMontageBlendOut()
{
    // 상황에 따라 BlendOut에서 종료할지 Completed에서 종료할지 결정
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UBaseGameplayAbility::OnMontageInterrupted()
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UBaseGameplayAbility::OnMontageCancelled()
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
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
