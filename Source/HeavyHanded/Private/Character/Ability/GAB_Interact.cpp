// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/Ability/GAB_Interact.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Character/BaseCharacter.h"

UGAB_Interact::UGAB_Interact()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGAB_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 2. 서버 권한 체크 (상호작용 판정은 서버에서 수행)
    if (ActorInfo->IsNetAuthority())
    {
        PerformInteraction();
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
                // 1. 물리 시뮬레이션 및 콜리전 끄기
                if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(HitActor->GetRootComponent()))
                {
                    PrimComp->SetSimulatePhysics(false);
                    PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                }

                // 2. 캐릭터 손 소켓에 부착
                HitActor->AttachToComponent(
                    Character->GetMesh(),
                    FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                    FName("Hand_R_Socket")
                );

                Character->SetHeldActor(HitActor);
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