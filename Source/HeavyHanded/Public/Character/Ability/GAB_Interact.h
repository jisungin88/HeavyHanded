// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Character/BaseGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GAB_Interact.generated.h"

UCLASS()
class HEAVYHANDED_API UGAB_Interact : public UBaseGameplayAbility
{
    GENERATED_BODY()

public:
    UGAB_Interact();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
    // 시선 기준 상호작용 사거리. 눈 위치에서 바라보는 방향으로 이만큼 훑는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
    float InteractionRange = 300.f;

    // 스윕 구체 반경. 크면 조준이 관대해지지만 엉뚱한 대상이 걸리기 쉬워진다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", Units = "cm"))
    float InteractionRadius = 30.f;

    // 다운 대상 부활에 필요한 채널링 시간 (E를 이만큼 누르고 있어야 한다)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Revive", meta = (ClampMin = "0.0", Units = "s"))
    float ReviveChannelDuration = 3.0f;

    // 상호작용 레이캐스트 및 로직 수행 함수
    void PerformInteraction();

    // 몽타주가 끝났을 때 호출될 콜백 함수
    UFUNCTION()
    void OnMontageFinished();

    // 부활 채널링 0.1초 반복 타이머 콜백. UFUNCTION 불필요 — GAB_Throw::UpdateTrajectoryPreview 와
    // 동일하게 SetTimer(this, &Func) 로 바로 바인딩한다.
    void TickReviveChannel();

    // SkillMontage(GA_Interact 블루프린트에 설정된 손짓 제스처)가 채널링 도중 먼저 끝나도
    // 어빌리티를 죽이지 않는다 — 부활 판정은 몽타주 길이가 아니라 채널링 타이머가 끝낸다.
    //
    // 비권위(클라이언트) 쪽도 함께 막아야 한다: LocalPredicted 실행이라 Press 한 번에 클라이언트
    // 예측 인스턴스와 서버 권위 인스턴스가 따로 생기는데, 클라이언트는 PerformInteraction 이 애초에
    // 안 돌아서 타이머가 항상 무효(0)다. 클라이언트도 몽타주는 로컬로 똑같이 재생하므로, 그쪽
    // 몽타주가 먼저 끝나 스스로 EndAbility(bReplicateEndAbility=true) 를 부르면 그게 서버로
    // 리플리케이트되어 서버 쪽의 진짜 채널링 인스턴스까지 강제 종료시켜버린다 — 실제로 발생을
    // 확인한 버그다. 그래서 "채널링 중"이 아니라 "채널링 중이거나 비권위"로 막는다.
    //
    // Loot/Door 등 다른 분기는 애초에 이 타이머가 안 걸리므로, 서버에서는 그대로 Super 를 타서
    // 기존과 동일하게 동작한다. (OnMontageInterrupted/OnMontageCancelled 는 건드리지 않는다 —
    // 몽타주 재생 실패 시의 강등 로직 등 특수 케이스가 있어 손대지 않는 게 안전하다.)
    virtual void OnMontageCompleted() override;
    virtual void OnMontageBlendOut() override;

private:
    FTimerHandle ReviveChannelTimerHandle;
    float ReviveChannelElapsed = 0.f;
    TWeakObjectPtr<class ABaseCharacter> ReviveChannelTarget;
    TWeakObjectPtr<class ABaseCharacter> ReviveChannelReviver;
};
