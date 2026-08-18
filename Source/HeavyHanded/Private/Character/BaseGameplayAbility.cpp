// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BaseGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Character/BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

// 어빌리티 진단용. 몽타주 재생 실패처럼 조용히 넘어가는 상황을 남긴다.
DEFINE_LOG_CATEGORY_STATIC(LogSkill, Log, All);

#if ENABLE_DRAW_DEBUG
// ──────────────────────────────────────────────────────────────
// [디버그 전용] 시각화 스위치
//
// ECVF_Cheat 라 쉬핑 빌드에서는 콘솔로 켤 수 없고,
// ENABLE_DRAW_DEBUG 가 0이면 그리기 코드 자체가 컴파일에서 빠진다.
// 게임 로직이 이 값을 읽어서는 안 된다 — 빌드 구성에 따라 동작이 달라진다.
// ──────────────────────────────────────────────────────────────
TAutoConsoleVariable<int32> CVarAbilityDebug(
	  TEXT("hh.Ability.Debug"),
	  0,
	  TEXT("0 = 끔, 1 = 상호작용 스윕, 2 = + 던지기 궤적"),
	  ECVF_Cheat);
#endif

UBaseGameplayAbility::UBaseGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UBaseGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    bSkillMontageStarted = false;

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 몽타주가 지정되지 않았으면 재생 태스크를 아예 만들지 않는다.
    //
    // UAbilityTask_PlayMontageAndWait 는 재생에 실패하면 Activate() 안에서 곧바로
    // OnCancelled 를 브로드캐스트한다(엔진 AbilityTask_PlayMontageAndWait.cpp:160).
    // ReadyForActivation() 이 동기 호출이라, 몽타주가 null 이면 Super::ActivateAbility
    // 가 반환되기도 전에 어빌리티가 취소 종료된다. 그러면 파생 클래스가 Super 뒤에
    // 이어서 하는 일이 전부 "이미 끝난 어빌리티" 위에서 벌어진다.
    // 특히 GAB_Throw 처럼 InputReleased 를 기다리는 어빌리티는 성립조차 하지 않는다.
    //
    // 몽타주가 없을 때 어빌리티를 언제 끝낼지는 파생 클래스의 몫이다.
    // IsPlayingSkillMontage() 로 판단할 수 있다.
    if (!SkillMontage)
    {
        return;
    }

    UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this,
        NAME_None,
        SkillMontage,
        1.0f,
        NAME_None,
        false);

    if (MontageTask)
    {
        MontageTask->OnCompleted.AddDynamic(this, &UBaseGameplayAbility::OnMontageCompleted);
        MontageTask->OnBlendOut.AddDynamic(this, &UBaseGameplayAbility::OnMontageBlendOut);
        MontageTask->OnInterrupted.AddDynamic(this, &UBaseGameplayAbility::OnMontageInterrupted);
        MontageTask->OnCancelled.AddDynamic(this, &UBaseGameplayAbility::OnMontageCancelled);

        // ReadyForActivation() 이 동기적으로 취소를 낼 수 있으므로 플래그를 먼저 세운다.
        bSkillMontageStarted = true;

        // 이 구간에서 오는 취소는 "재생 자체가 시작되지 못했다"는 뜻이다.
        // OnMontageCancelled 가 이 플래그를 보고 어빌리티를 끝낼지 말지 가른다.
        bMontageActivationInProgress = true;
        MontageTask->ReadyForActivation();
        bMontageActivationInProgress = false;
    }
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
    // ReadyForActivation() 안에서 곧바로 돌아온 취소는 "재생이 시작되지도 못했다"는 뜻이다.
    // 에셋이 비어 있거나 로드에 실패했거나, 메시에 AnimInstance 가 없거나,
    // 몽타주 스켈레톤이 맞지 않는 경우가 여기에 해당한다.
    // 이걸 일반 취소로 처리하면 어빌리티가 시작하자마자 죽어버리므로,
    // 몽타주만 없는 것으로 강등하고 어빌리티는 계속 진행시킨다.
    if (bMontageActivationInProgress)
    {
        bSkillMontageStarted = false;

        UE_LOG(LogSkill, Warning,
            TEXT("%s: SkillMontage(%s) 재생에 실패했다. 몽타주 없이 진행한다. "
                 "(에셋 누락 / 메시에 AnimInstance 없음 / 스켈레톤 불일치 확인)"),
            *GetName(), *GetNameSafe(SkillMontage));
        return;
    }

    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UBaseGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // InstancedPerActor 라 같은 인스턴스가 재사용된다. 다음 활성화에 값이 새지 않게 정리한다.
    bSkillMontageStarted = false;

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
