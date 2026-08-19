// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Ability/GAB_Interact.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/BaseCharacter.h"
#include "Core/VanZone.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"

// 상호작용 진단용. 집기 실패는 예외 없이 조용히 넘어가므로 이유를 남긴다.
DEFINE_LOG_CATEGORY_STATIC(LogInteract, Log, All);

namespace
{
	// 노획물 판정은 Loot.Type 하위 태그로 한다 — Config/Tags/Loot.ini (담당: 김민준).
	// HasTag 는 부모 매칭이라 Loot.Type.Heavy 등 하위 태그가 전부 걸린다.
	const FName LootTypeRootTagName(TEXT("Loot.Type"));

	// TODO(임시 폴백): 김민준의 ALootBase / ICarryable 이 develop 에 머지되면 제거한다.
	//   현재 프로젝트에는 IGameplayTagAssetInterface 를 구현한 액터가 하나도 없어서
	//   (Source/HeavyHanded/*/Loot/ 는 .gitkeep 뿐) GameplayTag 판정만 남기면
	//   테스트 맵에서 아무것도 집히지 않는다.
	const FName LegacyItemActorTagName(TEXT("Item"));

	// TODO: 문 상호작용은 오유석 담당 영역이고 대응 GameplayTag 가 아직 없다.
	//   (Hazard.Cycle.FireDoor 는 방화문 전용이라 일반 문에는 못 쓴다)
	const FName LegacyDoorActorTagName(TEXT("Door"));

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
    if (!IsPlayingSkillMontage())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGAB_Interact::OnMontageFinished()
{
    // 현재 활성화된 스펙 핸들과 액터 정보를 안전하게 넘겨서 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGAB_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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

        // 1. 집을 수 있는 노획물인 경우 (Loot.Type 하위 태그)
        if (IsCarryableLoot(HitActor))
        {
            if (Character->GetHeldActor())
            {
                UE_LOG(LogInteract, Log, TEXT("이미 %s 를 들고 있어 %s 를 집지 않는다."),
                    *GetNameSafe(Character->GetHeldActor()), *HitActor->GetName());
                return;
            }

            // 물리 끄기 -> 부착 순서와 실패 검증은 전부 SetHeldActor 가 처리한다.
            // 클라이언트에서도 OnRep_HeldActor 가 같은 경로를 밟는다.
            if (Character->SetHeldActor(HitActor))
            {
                UE_LOG(LogInteract, Log, TEXT("집기 성공: %s"), *HitActor->GetName());
            }
        }
        // 2. 밴 — 승차 / 하차 (담당: 지성인)
        //
        // TODO(구조): 여기서 코어 루프 클래스를 직접 캐스트하는 것은 임시다.
        //   상호작용 대상이 늘어날 때마다(문·좌대·금고·협력 장치) 이 사슬이 계속 길어지고,
        //   그때마다 남의 파트가 이 파일을 고쳐야 한다. 대상 쪽이 구현하는 인터페이스
        //   (IInteractable::OnInteract)로 뒤집어야 하는데, 그건 담당 영역을 여럿 건너는
        //   결정이라 전영배와 합의한 뒤에 한다.
        //
        //   그때까지 밴 쪽 조건과 처리는 전부 AVanZone 안에 있다 — 여기는 넘겨주기만 한다.
        else if (AVanZone* Van = Cast<AVanZone>(HitActor))
        {
            Van->TryToggleBoarding(Character);
        }
        // 3. "Door" 태그가 붙어있는 경우 (아직 GameplayTag 대응 없음 — 위 TODO 참조)
        else if (HitActor->ActorHasTag(LegacyDoorActorTagName))
        {
            UE_LOG(LogInteract, Log, TEXT("문 상호작용: %s"), *HitActor->GetName());
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