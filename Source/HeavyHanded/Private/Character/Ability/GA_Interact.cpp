// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Ability/GA_Interact.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "BaseCharacter.h"

UGA_Interact::UGA_Interact()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 1. 코스트 및 쿨다운 검사
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 2. 서버 권한 체크 (상호작용 판정은 서버에서 수행)
    if (ActorInfo->IsNetAuthority())
    {
        PerformInteraction();
    }

    // 3. 네트워크 동기화가 보장되는 몽타주 재생 태스크 사용
    if (SkillMontage)
    {
        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this,
            NAME_None,
            SkillMontage,
            1.0f,
            NAME_None,
            false
        );

        if (MontageTask)
        {
            // 몽타주 상태에 따른 종료 바인딩
            MontageTask->OnCompleted.AddDynamic(this, &UGA_Interact::OnMontageFinished);
            MontageTask->OnBlendOut.AddDynamic(this, &UGA_Interact::OnMontageFinished);
            MontageTask->OnInterrupted.AddDynamic(this, &UGA_Interact::OnMontageFinished);
            MontageTask->OnCancelled.AddDynamic(this, &UGA_Interact::OnMontageFinished);

            MontageTask->ReadyForActivation();
        }
        else
        {
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        }
    }
    else
    {
        // 몽타주가 없으면 즉시 종료 처리하여 스킬이 잠기는 것 방지
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UGA_Interact::OnMontageFinished()
{
    // 현재 활성화된 스펙 핸들과 액터 정보를 안전하게 넘겨서 어빌리티 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Interact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Interact::PerformInteraction()
{
    ABaseCharacter* Character = Cast<ABaseCharacter>(GetCurrentActorInfo()->AvatarActor.Get());
    //ACharacter* Character = Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get());

    if (!Character) return;

    FVector StartLocation = Character->GetActorLocation();
    FVector ForwardVector = Character->GetActorForwardVector();
    FVector EndLocation = StartLocation + (ForwardVector * 300.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Character);


    // 라인 대신 약간의 반경을 준 스피어 트레이스 (판정 여유)
    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        FQuat::Identity,
        ECC_Visibility,
        FCollisionShape::MakeSphere(50.0f),
        QueryParams
    );

    DrawDebugSphere(GetWorld(), StartLocation, 50.0f, 12, FColor::Yellow, false, 2.0f);
    DrawDebugSphere(GetWorld(), EndLocation, 50.0f, 12, FColor::Yellow, false, 2.0f);
    DrawDebugLine(GetWorld(), StartLocation, bHit ? HitResult.Location : EndLocation,
        bHit ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.5f);
  
    if (bHit)
    {
        DrawDebugSphere(GetWorld(), HitResult.Location, 10.0f, 12, FColor::Blue, false, 2.0f);
    }

    if (bHit && HitResult.GetActor())
    {
        AActor* HitActor = HitResult.GetActor();

        // 1. "Item" 태그가 붙어있는 경우
        if (HitActor->ActorHasTag(FName("Item")))
        {
            UE_LOG(LogTemp, Log, TEXT("Item Get: %s"), *HitActor->GetName());
            // 이미 뭔가 들고 있지 않다면 집어 들기 실행
            if (!Character->GetHeldActor())
            {
                Character->Multicast_AttachItem(HitActor);
                UE_LOG(LogTemp, Log, TEXT("Item Interact success: %s"), *HitActor->GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Item ded"));
            }
        }
        // 2. "Door" 태그가 붙어있는 경우
        else if (HitActor->ActorHasTag(FName("Door")))
        {
            UE_LOG(LogTemp, Log, TEXT("Door Open/Close: %s"), *HitActor->GetName());
        }
        // 3. 태그가 없거나 다른 사물이 맞은 경우
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No: %s"), *HitActor->GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PerformInteraction: No interactable object found."));
    }
}