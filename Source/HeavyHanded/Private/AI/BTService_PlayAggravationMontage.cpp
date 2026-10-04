#include "AI/BTService_PlayAggravationMontage.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/GuardCharacter.h"

UBTService_PlayAggravationMontage::UBTService_PlayAggravationMontage()
{
	NodeName = TEXT("Play Aggravation Montage");
	bCreateNodeInstance = true;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

void UBTService_PlayAggravationMontage::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	ActiveMontage = nullptr;
	if (AGuardCharacter* GuardCharacter = GetGuardCharacter(OwnerComp); IsValid(GuardCharacter) && GuardCharacter->HasAuthority())
	{
		ActiveMontage = GuardCharacter->GetAggravationMontage();
		if (!IsValid(ActiveMontage) && GuardCharacter->GuardType != EGuardType::Dog)
		{
			ActiveMontage = AggravationMontage;
		}
		if (IsValid(ActiveMontage))
		{
			GuardCharacter->SetAggravationMontage(ActiveMontage, true, MontageBlendOutTime);
		}
	}
}

void UBTService_PlayAggravationMontage::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
	if (AGuardCharacter* GuardCharacter = GetGuardCharacter(OwnerComp); IsValid(GuardCharacter) && GuardCharacter->HasAuthority() && IsValid(ActiveMontage))
	{
		GuardCharacter->SetAggravationMontage(ActiveMontage, false, MontageBlendOutTime);
	}
	ActiveMontage = nullptr;
}

AGuardCharacter* UBTService_PlayAggravationMontage::GetGuardCharacter(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	return IsValid(AIController) ? Cast<AGuardCharacter>(AIController->GetPawn()) : nullptr;
}
