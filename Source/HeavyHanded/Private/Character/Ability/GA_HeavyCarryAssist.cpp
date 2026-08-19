#include "Character/Ability/GA_HeavyCarryAssist.h"
#include "Character/BaseCharacter.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeavyCarry, Log, All);

UGA_HeavyCarryAssist::UGA_HeavyCarryAssist()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

	// 텐션 판정이 이동 입력(서버 권위값)에 의존하므로 서버에서만 돈다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// "Ability.HeavyCarryAssist" 이벤트가 오면 자동으로 활성화되도록 등록.
	// GAB_Interact 는 이 태그로 HandleGameplayEvent 만 부르고, 나머지는 여기서 처리한다.
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = FGameplayTag::RequestGameplayTag(FName("Ability.HeavyCarryAssist"));
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);

	// 같은 태그를 어빌리티 식별 태그로도 등록한다 - GAB_Interact 가 보조자의 자발적
	// 해제를 CancelAbilities(WithTags=이 태그)로 찾아 취소할 때 쓴다.
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.HeavyCarryAssist")));
}

void UGA_HeavyCarryAssist::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	Assistant = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get());
	AActor* Item = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;

	if (!Assistant || !IsValid(Item))
	{
		UE_LOG(LogHeavyCarry, Warning, TEXT("보조 대상이 유효하지 않아 취소한다."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Item 을 붙잡고 있는 주 운반자를 부착 관계로 역추적한다.
	const USceneComponent* Root = Item ? Item->GetRootComponent() : nullptr;

	if (!Root)
	{
		UE_LOG(LogHeavyCarry, Warning, TEXT("Item의 RootComponent가 유효하지 않아 취소한다."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const USceneComponent* AttachParent = Root ? Root->GetAttachParent() : nullptr;
	ABaseCharacter* Primary = AttachParent ? Cast<ABaseCharacter>(AttachParent->GetOwner()) : nullptr;

	if (!Primary || Primary->GetHeldActor() != Item || Primary == Assistant || Primary->GetHeavyCarryAssistant())
	{
		UE_LOG(LogHeavyCarry, Log, TEXT("%s 는 지금 보조할 수 없다 (주 운반자 없음/자기 자신/이미 보조자 있음)."),
			*GetNameSafe(Item));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PrimaryCarrier = Primary;
	CarriedItem = Item;

	Primary->SetHeavyCarryAssistant(Assistant);
	Assistant->SetAssistingPrimaryCarrier(Primary, Item);
	Primary->RemoveHeavySoloPenalty();

	// AnimBP 가 폴링하는 Idle/Walk 블렌드스페이스 선택용 상태 — 둘 다 Coop 로 전환한다.
	Primary->SetHeavyCarryState(EHeavyCarryState::Coop);
	Assistant->SetHeavyCarryState(EHeavyCarryState::Coop);

	if (CoopCarryStateEffectClass)
	{
		Primary->ApplyGameplayEffectToSelf(CoopCarryStateEffectClass);
		Assistant->ApplyGameplayEffectToSelf(CoopCarryStateEffectClass);
	}

	UE_LOG(LogHeavyCarry, Log, TEXT("%s 가 %s 의 %s 운반을 돕기 시작"),
		*GetNameSafe(Assistant), *GetNameSafe(Primary), *GetNameSafe(Item));

	MonitorTask = UAT_MonitorHeavyCarry::MonitorHeavyCarry(
		this, Assistant, PrimaryCarrier, CarriedItem,
		MaxAssistDistance, TugOpposingDotThreshold, TugTensionBuildRate, TugTensionDecayRate, TugTensionKnockbackThreshold);

	MonitorTask->OnEnded.AddDynamic(this, &UGA_HeavyCarryAssist::HandleAssistEnded);
	MonitorTask->ReadyForActivation();

	// 주 운반자가 물건을 놓는 순간(BaseCharacter::SetHeldActor) 즉시 통지받아
	// MonitorTask 의 다음 틱 폴링을 기다리지 않고 바로 정리한다.
	static const FGameplayTag PrimaryReleasedEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Loot.Dropped"));
	UAbilityTask_WaitGameplayEvent* WaitReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, PrimaryReleasedEventTag, nullptr, /*OnlyTriggerOnce*/ true);
	WaitReleaseTask->EventReceived.AddDynamic(this, &UGA_HeavyCarryAssist::HandleImmediatePrimaryRelease);
	WaitReleaseTask->ReadyForActivation();
}

void UGA_HeavyCarryAssist::HandleAssistEnded(EHeavyCarryEndReason Reason)
{
	CleanupCrossReferences();

	if (Reason != EHeavyCarryEndReason::Knockback)
	{
		// 거리 초과 / 주 운반자가 물건을 놓음 -> 조용히 어빌리티만 끝낸다.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// --- 넉백 ---
	if (!Assistant || !PrimaryCarrier)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FVector KnockDirection = (Assistant->GetActorLocation() - PrimaryCarrier->GetActorLocation()).GetSafeNormal2D();

	// ACharacter::LaunchCharacter 는 CharacterMovementComponent 를 통해 자동으로
	// 모든 클라이언트에 리플리케이트된다 - 별도 Multicast RPC 가 필요 없다.
	Assistant->LaunchCharacter(KnockDirection * KnockbackLaunchStrength, true, false);

	if (StaggeredMovementLockEffectClass)
	{
		Assistant->ApplyGameplayEffectToSelf(StaggeredMovementLockEffectClass);
	}

	if (KnockbackStaggerMontage)
	{
		// AbilityTask_PlayMontageAndWait 는 ASC 를 통해 몽타주를 자동으로 리플리케이트한다
		// (GAB_Interact 가 이미 쓰고 있는 것과 동일한 패턴).
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, KnockbackStaggerMontage);

		MontageTask->OnCompleted.AddDynamic(this, &UGA_HeavyCarryAssist::OnStaggerMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UGA_HeavyCarryAssist::OnStaggerMontageFinished);
		MontageTask->OnCancelled.AddDynamic(this, &UGA_HeavyCarryAssist::OnStaggerMontageFinished);
		MontageTask->ReadyForActivation();
	}
	else
	{
		OnStaggerMontageFinished();
	}
}

void UGA_HeavyCarryAssist::OnStaggerMontageFinished()
{
	if (Assistant && StaggeredMovementLockEffectClass)
	{
		Assistant->RemoveGameplayEffectFromSelf(StaggeredMovementLockEffectClass);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HeavyCarryAssist::HandleImmediatePrimaryRelease(FGameplayEventData Payload)
{
	// 다른 아이템을 놓은 이벤트라면 무시한다 (이론상 발생하지 않지만 방어적으로 확인).
	if (Payload.Target.Get() != CarriedItem.Get())
	{
		return;
	}

	if (MonitorTask)
	{
		MonitorTask->EndWithReason(EHeavyCarryEndReason::PrimaryReleased);
	}
}

void UGA_HeavyCarryAssist::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 정상 경로(HandleAssistEnded)를 거치지 않고 여기로 바로 들어온 경우
	// (자발적 해제로 인한 CancelAbilities, 캐릭터 사망, 접속 종료 등) — 직접 정리한다.
	if (!bCrossReferencesCleaned)
	{
		if (MonitorTask)
		{
			MonitorTask->EndTask();
		}
		CleanupCrossReferences();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_HeavyCarryAssist::CleanupCrossReferences()
{
	// 이미 정리됐으면 다시 돌지 않는다 — 특히 솔로 페널티 GE가 중복 적용되는 것을 막는다.
	if (bCrossReferencesCleaned)
	{
		return;
	}
	bCrossReferencesCleaned = true;

	if (PrimaryCarrier)
	{
		PrimaryCarrier->ClearHeavyCarryAssistant();

		if (CoopCarryStateEffectClass)
		{
			PrimaryCarrier->RemoveGameplayEffectFromSelf(CoopCarryStateEffectClass);
		}

		// 아직 물건을 들고 있다면(거리초과/넉백이지 주 운반자가 놓은 게 아니라면)
		// 다시 혼자-운반 페널티를 건다.
		if (PrimaryCarrier->GetHeldActor())
		{
			PrimaryCarrier->ApplyHeavySoloPenalty();
			PrimaryCarrier->SetHeavyCarryState(EHeavyCarryState::Solo);
		}
		// 이미 물건을 놓은 경우엔 SetHeldActor 가 HeavyCarryState 를 None 으로 이미 바꿔놨다.
	}

	if (Assistant)
	{
		Assistant->ClearAssistingPrimaryCarrier();
		Assistant->SetHeavyCarryState(EHeavyCarryState::None);

		if (CoopCarryStateEffectClass)
		{
			Assistant->RemoveGameplayEffectFromSelf(CoopCarryStateEffectClass);
		}
	}
}
