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

    // 서버 전용 진입점. ABaseCharacter::Server_CancelRevive_Implementation 이 이 캐릭터의
    // GAB_Interact 인스턴스(InstancedPerActor 라 하나뿐)를 찾아서 호출한다.
    // 실제로 채널링 중이 아니었으면(일반 상호작용 릴리즈에서도 호출될 수 있다) 조용히 무시한다.
    void CancelReviveChannel();

    // ABaseCharacter::OnRep_ReviveChannelActive 가 부른다. 리바이버 본인 클라이언트의
    // 로컬 예측 몽타주 인스턴스에 서버와 같은 Hold 링크를 걸어준다 — 1인칭이라 본인
    // 팔 메시가 보이므로, 서버가 PerformInteraction/EndReviveChannel 에서 자기 인스턴스에
    // 건 것과 똑같은 걸 클라이언트 쪽에도 걸어줘야 한다. 서버(권위) 인스턴스에서 호출되면
    // 아무것도 하지 않는다 — 그쪽은 이미 직접 처리했다.
    void SetLocalHoldState(bool bHold);

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

    // 조준 유지 중 정지 루프를 도는 구간 이름 (SkillMontage 안에 존재해야 함).
    // GAB_Throw 와 같은 이름 규칙 — 일반 상호작용(줍기/문)에서는 몽타주가 기본
    // 다음 섹션(에디터에서 지정)을 타고 HoldEnd 로 곧장 흘러간다. 코드가 손대는 건
    // 부활 채널링 시작/종료 시점뿐이다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Revive")
    FName HoldStartSectionName = TEXT("HoldStart");

    // 손을 뗐거나(일반 상호작용) 채널이 끝났을 때(부활) 이어질 회수 구간 이름.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Revive")
    FName HoldEndSectionName = TEXT("HoldEnd");

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

    // 채널 종료 지점 통합. 정상 완료 · 거리 이탈 · 대상 회복 · 리바이버 다운 · 입력 릴리즈 ·
    // 서버 RPC 취소까지 전부 이 함수 하나로 모은다 — 타이머 정리, 진행률 리셋,
    // 몽타주 Hold 해제(포즈 복귀)를 여기 한 곳에서만 하므로 새 종료 경로가 생겨도
    // 이 함수만 부르면 포즈가 빠지지 않는 사고를 막을 수 있다.
    void EndReviveChannel();
};
