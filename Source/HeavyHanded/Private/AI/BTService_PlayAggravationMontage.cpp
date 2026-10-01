// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTService_PlayAggravationMontage.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

#include "AI/GuardAnimInstance.h"

UBTService_PlayAggravationMontage::UBTService_PlayAggravationMontage()
{
	NodeName = TEXT("Play Aggravation Montage");
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

void UBTService_PlayAggravationMontage::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (!IsValid(AggravationMontage))
	{
		return;
	}

	if (UGuardAnimInstance* AnimInstance = GetGuardAnimInstance(OwnerComp))
	{
		AnimInstance->Montage_Play(AggravationMontage);
	}
}

void UBTService_PlayAggravationMontage::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	if (!IsValid(AggravationMontage))
	{
		return;
	}

	if (UGuardAnimInstance* AnimInstance = GetGuardAnimInstance(OwnerComp);
		IsValid(AnimInstance) && AnimInstance->Montage_IsPlaying(AggravationMontage))
	{
		AnimInstance->Montage_Stop(MontageBlendOutTime, AggravationMontage);
	}
}

UGuardAnimInstance* UBTService_PlayAggravationMontage::GetGuardAnimInstance(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!IsValid(AIController))
	{
		return nullptr;
	}

	ACharacter* GuardCharacter = Cast<ACharacter>(AIController->GetPawn());
	if (!IsValid(GuardCharacter) || !IsValid(GuardCharacter->GetMesh()))
	{
		return nullptr;
	}

	return Cast<UGuardAnimInstance>(GuardCharacter->GetMesh()->GetAnimInstance());
}

