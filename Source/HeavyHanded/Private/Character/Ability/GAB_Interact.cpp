// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Ability/GAB_Interact.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/BaseCharacter.h"
#include "Core/VanZone.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Interfaces/Carryable.h"        // GetPrimaryCarrier 로 "이미 들려 있는가" 를 판정한다
#include "Interfaces/Interactable.h"        // GetPrimaryCarrier 로 "이미 들려 있는가" 를 판정한다

// 상호작용 진단용. 집기 실패는 예외 없이 조용히 넘어가므로 이유를 남긴다.
DEFINE_LOG_CATEGORY_STATIC(LogInteract, Log, All);

namespace
{
	// 노획물 판정은 Loot.Type 하위 태그로 한다 — Config/Tags/Loot.ini (담당: 김민준).
	// HasTag 는 부모 매칭이라 Loot.Type.Heavy 등 하위 태그가 전부 걸린다.
	const FName LootTypeRootTagName(TEXT("Loot.Type"));

	// 폴백: ALootBase 가 IGameplayTagAssetInterface 로 Loot.Type 을 내놓게 됐으므로
	//   (develop 머지 완료) 진짜 노획물은 위 태그 경로로 전부 걸린다.
	//   이 태그는 이제 태그가 없는 테스트 맵 액터 전용이다.
	//   테스트 맵의 임시 액터가 정리되면 이 줄과 아래 3번 분기를 같이 지운다. [전영배]
	const FName LegacyItemActorTagName(TEXT("Item"));

	// TODO: 문 상호작용은 오유석 담당 영역이고 대응 GameplayTag 가 아직 없다.
	//   (Hazard.Cycle.FireDoor 는 방화문 전용이라 일반 문에는 못 쓴다)
	const FName LegacyDoorActorTagName(TEXT("Door"));

	// 보조자가 이미 돕고 있는 중량물에 다시 상호작용하면(자발적 해제) 이 태그로
	// GA_HeavyCarryAssist 를 CancelAbilities 로 찾아 끝낸다.
	const FName HeavyCarryAssistAbilityTagName(TEXT("Ability.HeavyCarryAssist"));

	//08.18 추가
	const FName LootTypeHeavyTagName(TEXT("Loot.Type.Heavy"));
	bool IsHeavyLoot(const AActor* Actor)
	{
		if (!Actor) return false;

		if (const IGameplayTagAssetInterface* TagOwner = Cast<const IGameplayTagAssetInterface>(Actor))
		{
			const FGameplayTag HeavyTag = FGameplayTag::RequestGameplayTag(LootTypeHeavyTagName, false);
			if (HeavyTag.IsValid())
			{
				FGameplayTagContainer OwnedTags;
				TagOwner->GetOwnedGameplayTags(OwnedTags);
				return OwnedTags.HasTag(HeavyTag);
			}
		}
		return false;
	}
	//08.18 end

	// 다운 상태 판정 — 새 태그가 아니라 이미 있는 State.Downed(전영배 / Config/Tags/State.ini)를
	// 조회만 한다. 대상이 IAbilitySystemInterface 를 구현하면(플레이어는 PlayerState 를 통해
	// 항상 그렇다) 그 ASC 에 태그가 붙어 있는지만 본다.
	bool IsDownedTarget(const AActor* Actor)
	{
		if (!Actor) return false;

		if (const IAbilitySystemInterface* ASI = Cast<const IAbilitySystemInterface>(Actor))
		{
			if (UAbilitySystemComponent* TargetASC = ASI->GetAbilitySystemComponent())
			{
				return TargetASC->HasMatchingGameplayTag(HHTags::State_Downed);
			}
		}
		return false;
	}

	// 집을 수 있는 노획물인지 판정
	bool IsCarryableLoot(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}

		if (const IGameplayTagAssetInterface* TagOwner = Cast<const IGameplayTagAssetInterface>(Actor))
		{
			// 태그 매니저 초기화 이후에 조회해야 하므로 정적 초기화로 캐싱하지 않는다
			const FGameplayTag LootTypeRoot =
				FGameplayTag::RequestGameplayTag(LootTypeRootTagName, /*ErrorIfNotFound*/ false);

			if (LootTypeRoot.IsValid())
			{
				FGameplayTagContainer OwnedTags;
				TagOwner->GetOwnedGameplayTags(OwnedTags);

				if (OwnedTags.HasTag(LootTypeRoot))
				{
					return true;
				}
			}
		}

		return Actor->ActorHasTag(LegacyItemActorTagName);
	}
}

UGAB_Interact::UGAB_Interact()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGAB_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // Commit 실패 등으로 Super 에서 이미 종료됐으면 더 진행하지 않는다.
    if (!IsActive())
    {
        return;
    }

    // 2. 서버 권한 체크 (상호작용 판정은 서버에서 수행)
    if (ActorInfo->IsNetAuthority())
    {
        PerformInteraction();
    }

    // 몽타주가 없으면 어빌리티를 끝내줄 콜백이 없다.
    // 상호작용은 한 번에 끝나는 동작이므로 여기서 바로 종료한다.
    // 단, 부활 채널링 타이머가 막 시작됐다면 그 타이머가 어빌리티 수명을 이어받는다.
    if (!IsPlayingSkillMontage() && !ReviveChannelTimerHandle.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGAB_Interact::OnMontageFinished()
{
    // 현재 활성화된 스펙 핸들과 액터 정보를 안전하게 넘겨서 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGAB_Interact::TickReviveChannel()
{
    ABaseCharacter* Target = ReviveChannelTarget.Get();
    ABaseCharacter* Reviver = ReviveChannelReviver.Get();

    const bool bValid = IsValid(Target) && IsValid(Reviver);
    const bool bInRange = bValid && FVector::Dist(Reviver->GetActorLocation(), Target->GetActorLocation()) <= InteractionRange;
    const bool bStillDowned = bValid && Target->IsDowned();

    if (!bValid || !bInRange || !bStillDowned)
    {
        GetWorld()->GetTimerManager().ClearTimer(ReviveChannelTimerHandle);
        if (bValid)
        {
            Target->SetReviveProgress(0.f);
        }
        ReviveChannelElapsed = 0.f;

        UE_LOG(LogInteract, Log, TEXT("부활 채널링 취소 (유효=%d 사거리=%d 다운유지=%d)"), bValid, bInRange, bStillDowned);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    ReviveChannelElapsed += 0.1f;
    Target->SetReviveProgress(ReviveChannelElapsed / ReviveChannelDuration);

    if (ReviveChannelElapsed >= ReviveChannelDuration)
    {
        GetWorld()->GetTimerManager().ClearTimer(ReviveChannelTimerHandle);

        // 기존 즉시 실행 분기에 있던 제거 코드 그대로. 상호작용을 건 쪽이 대상의 GE 클래스를
        // 알 필요가 없도록 소스이펙트가 아닌 태그로 지운다.
        if (UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent())
        {
            TargetASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(HHTags::State_Downed));
            UE_LOG(LogInteract, Log, TEXT("%s 를 다운 상태에서 복구시켰다."), *Target->GetName());
        }
        Target->SetReviveProgress(0.f);
        ReviveChannelElapsed = 0.f;

        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGAB_Interact::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);

    if (ReviveChannelTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReviveChannelTimerHandle);
        if (ABaseCharacter* Target = ReviveChannelTarget.Get())
        {
            Target->SetReviveProgress(0.f);
        }
        ReviveChannelElapsed = 0.f;

        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }
}

void UGAB_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 정상 경로(TickReviveChannel)를 거치지 않고 여기로 바로 들어온 경우
    // (외부 강제 취소, 캐릭터 사망 등) 대비 안전망. ClearTimer 는 핸들 자체를 무효화하므로
    // 정상 경로에서 이미 정리된 뒤라면 아래 IsValid() 가 걸러줘서 중복 실행되지 않는다.
    if (ReviveChannelTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReviveChannelTimerHandle);
        if (ABaseCharacter* Target = ReviveChannelTarget.Get())
        {
            Target->SetReviveProgress(0.f);
        }
        ReviveChannelElapsed = 0.f;
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGAB_Interact::OnMontageCompleted()
{
    // 채널링 중(서버)이거나 비권위(클라이언트 예측 인스턴스)면 몽타주가 먼저 끝나도
    // 스스로 EndAbility 를 부르지 않는다. 클라이언트가 자기 로컬 몽타주 타이밍만으로
    // EndAbility(bReplicateEndAbility=true) 를 부르면 그게 서버로 리플리케이트되어
    // 서버 쪽의 진짜 채널링 인스턴스까지 강제 종료시켜버리기 때문 — 실제로 재현된 버그다.
    if (ReviveChannelTimerHandle.IsValid() || (GetCurrentActorInfo() && !GetCurrentActorInfo()->IsNetAuthority()))
    {
        return;
    }
    Super::OnMontageCompleted();
}

void UGAB_Interact::OnMontageBlendOut()
{
    if (ReviveChannelTimerHandle.IsValid() || (GetCurrentActorInfo() && !GetCurrentActorInfo()->IsNetAuthority()))
    {
        return;
    }
    Super::OnMontageBlendOut();
}

void UGAB_Interact::PerformInteraction()
{
    ABaseCharacter* Character = Cast<ABaseCharacter>(GetCurrentActorInfo()->AvatarActor.Get());
    //ACharacter* Character = Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get());

    if (!Character) return;

    // 밴에 타고 있으면 조준을 보지 않고 내린다 (담당: 지성인).
    // 탑승하면 몸이 밴에 고정되므로, 하차에 조준을 요구하면 차 안에서 벽을 겨눠야 내릴 수 있다.
    // 스윕보다 위에 있는 이유가 이것이다 — 밴 안에서 할 수 있는 일은 내리는 것뿐이다.
    if (AVanZone::TryDisembarkIfBoarded(Character))
    {
        return;
    }

    // 캡슐 중심에서 액터 전방으로 훑으면 시선 피치가 반영되지 않는다.
    // (bUseControllerRotationYaw 만 켜져 있어 액터 전방은 항상 수평이다)
    // 그러면 가슴 높이 띠만 검사하게 되어 바닥에 놓인 물건은 영영 집을 수 없다 —
    // 던진 아이템을 다시 못 줍던 원인이 이것이었다.
    //
    // GetActorEyesViewPoint 는 서버에서도 원격 클라이언트의 시선을 돌려준다
    // (피치가 RemoteViewPitch 로 복제된다. 정밀도는 낮지만 상호작용 판정엔 충분하다).
    FVector EyeLocation;
    FRotator EyeRotation;
    Character->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    const FVector StartLocation = EyeLocation;
    const FVector EndLocation = StartLocation + EyeRotation.Vector() * InteractionRange;

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Character);

    // 라인 대신 약간의 반경을 준 스피어 트레이스 (판정 여유)
    const bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        FQuat::Identity,
        ECC_Visibility,
        FCollisionShape::MakeSphere(InteractionRadius),
        QueryParams
    );

#if ENABLE_DRAW_DEBUG
    // [디버그 전용] 상호작용 스윕 시각화. hh.Ability.Debug 1
    if (CVarAbilityDebug.GetValueOnGameThread() > 0)
    {
        DrawDebugSphere(GetWorld(), StartLocation, InteractionRadius, 12, FColor::Yellow, false, 2.0f);
        DrawDebugSphere(GetWorld(), EndLocation, InteractionRadius, 12, FColor::Yellow, false, 2.0f);
        DrawDebugLine(GetWorld(), StartLocation, bHit ? HitResult.Location : EndLocation,
            bHit ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.5f);

        if (bHit)
        {
            DrawDebugSphere(GetWorld(), HitResult.Location, 10.0f, 12, FColor::Blue, false, 2.0f);
        }
    }
#endif

    if (bHit && HitResult.GetActor())
    {
        AActor* HitActor = HitResult.GetActor();

        // 0. 대상이 다운 상태인 경우 — 부활. 즉시 처리하지 않고 0.1초 반복 타이머로 채널링을
        //    시작한다. 실제 태그 제거(RemoveActiveEffectsWithGrantedTags)는 TickReviveChannel 이
        //    ReviveChannelDuration 을 다 채웠을 때 실행한다.
        if (IsDownedTarget(HitActor))
        {
            if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(HitActor))
            {
                ReviveChannelTarget = TargetChar;
                ReviveChannelReviver = Character;
                ReviveChannelElapsed = 0.f;

                GetWorld()->GetTimerManager().SetTimer(
                    ReviveChannelTimerHandle, this, &UGAB_Interact::TickReviveChannel, 0.1f, /*bLoop*/ true);

                UE_LOG(LogInteract, Log, TEXT("%s 부활 채널링 시작"), *HitActor->GetName());
            }
        }
        // 1. 집을 수 있는 노획물인 경우 (Loot.Type 하위 태그)
        else if (IsCarryableLoot(HitActor) || IsHeavyLoot(HitActor))
        {
            if (Character->GetHeldActor())
            {
                UE_LOG(LogInteract, Log, TEXT("이미 %s 를 들고 있어 %s 를 집지 않는다."),
                    *GetNameSafe(Character->GetHeldActor()), *HitActor->GetName());
                return;
            }

            if (Character->GetAssistedHeavyItem())
            {
                // 이미 돕고 있는 중량물에 다시 상호작용 = 자발적으로 그만 돕기.
                // GA_HeavyCarryAssist 는 AbilityTags 에 같은 태그를 등록해 두었으므로
                // CancelAbilities 가 정확히 이 어빌리티만 찾아 EndAbility(bWasCancelled=true) 시킨다.
                UE_LOG(LogInteract, Log, TEXT("%s 운반 돕기를 그만둔다."), *GetNameSafe(Character->GetAssistedHeavyItem()));

                if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
                {
                    const FGameplayTag AssistAbilityTag = FGameplayTag::RequestGameplayTag(HeavyCarryAssistAbilityTagName);
                    const FGameplayTagContainer CancelTags(AssistAbilityTag);
                    ASC->CancelAbilities(&CancelTags, nullptr, nullptr);
                }
                return;
            }

            const bool bHeavy = IsHeavyLoot(HitActor);

            // "이미 들려 있는가" 는 원래 Attach 여부로만 봤다. 중량형은 처음부터(물리 핸들
            // 시절도, 지금의 Kinematic 도) AttachToComponent 를 쓴 적이 없어서 이 판정이
            // 항상 false 로 나왔다 — 그래서 이미 1번이 들고 있어도 2번째 E 입력이 "새로
            // 집는다" 경로로 새서 CanBeCarriedBy 에 조용히 거부당했다(2번째 플레이어가
            // 못 집는 것처럼 보이던 원인).
            //
            // ICarryable 을 구현하는 노획물(일반·중량형 전부)은 PrimaryCarrier 로 정확히
            // 판정한다. ICarryable 을 구현하지 않는 레거시 액터("Item" 태그만 있는 것)는
            // 여전히 Attach 로만 운반 상태를 표현하므로 그 경로는 그대로 남겨 둔다.
            const ICarryable* HitCarryable = Cast<const ICarryable>(HitActor);
            const USceneComponent* HitRoot = HitActor->GetRootComponent();
            const bool bAlreadyCarried = HitCarryable
                ? IsValid(HitCarryable->GetPrimaryCarrier())
                : (HitRoot && HitRoot->GetAttachParent() != nullptr);

            if (bAlreadyCarried)
            {
                if (bHeavy)
                {
                    // ★ 직접 함수 호출 대신 GameplayEvent 로 GA_HeavyCarryAssist 를 발동시킨다.
                    const FGameplayTag& AssistEventTag = HHTags::Ability_HeavyCarryAssist;

                    FGameplayEventData EventData;
                    EventData.Target = HitActor;

                    if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent())
                    {
                        ASC->HandleGameplayEvent(AssistEventTag, &EventData);
                        UE_LOG(LogInteract, Log, TEXT("보조 운반 이벤트 발동: %s"), *HitActor->GetName());
                    }
                }
                else
                {
                    UE_LOG(LogInteract, Log, TEXT("%s 는 이미 다른 사람이 들고 있다."), *HitActor->GetName());
                }
                return;
            }

            // 물리 끄기 -> 부착 순서와 실패 검증은 전부 SetHeldActor 가 처리한다.
            // 클라이언트에서도 OnRep_HeldActor 가 같은 경로를 밟는다.
            if (Character->SetHeldActor(HitActor, bHeavy))
            {
                UE_LOG(LogInteract, Log, TEXT("집기 성공: %s (Heavy=%s)"), *HitActor->GetName(), bHeavy ? TEXT("O") : TEXT("X"));
            }
        }
		// 2. 밴 — 승차 / 하차 (담당: 지성인)
		// IInteractable 전환 중. AVanZone 이 IInteractable 을 구현하면 이 주석을 지우고
		// 위쪽 IInteractable 분기가 자연히 처리한다. 그 전까지만 남겨 둔다.
		//else if (AVanZone* Van = Cast<AVanZone>(HitActor))
		//{
		//    Van->TryToggleBoarding(Character);
		//}
		// 3. "Door" 태그가 붙어있는 경우 — 마찬가지로 IInteractable 전환 대상
		//else if (HitActor->ActorHasTag(LegacyDoorActorTagName))
		//{
		//    UE_LOG(LogInteract, Log, TEXT("문 상호작용: %s"), *HitActor->GetName());
		//}

		// 2. IInteractable 을 구현한 대상 — 문·상점·상자·밴 등 '집지 않는' 즉발성 상호작용.
		//    실제 동작은 전부 구현체(담당 팀원) 책임이고, GAB_Interact 는 대상 종류를 몰라도 된다.
		//    ImplementsInterface + Execute_ 조합을 써야 블루프린트로만 오버라이드한
		//    구현도 정상 호출된다 — Cast<IInteractable> 로 직접 부르면 C++ 기본 구현만 실행되고
		//    블루프린트 쪽 오버라이드는 조용히 무시된다.
		else if (HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
		{
			IInteractable::Execute_OnInteract(HitActor, Character);
			UE_LOG(LogInteract, Log, TEXT("IInteractable 상호작용: %s"), *HitActor->GetName());
		}
        // 4. 태그가 없거나 다른 사물이 맞은 경우
        else
        {
            UE_LOG(LogInteract, Log,
                TEXT("%s 는 상호작용 대상이 아니다. (Actor Tags 에 '%s' 가 있는지 확인)"),
                *HitActor->GetName(), *LegacyItemActorTagName.ToString());
        }
    }
    else
    {
        UE_LOG(LogInteract, Log, TEXT("시선 방향 %.0f 이내에 상호작용 대상이 없다."), InteractionRange);
    }
}
