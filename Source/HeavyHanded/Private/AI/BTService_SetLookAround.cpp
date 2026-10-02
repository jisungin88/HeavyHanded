#include "AI/BTService_SetLookAround.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/GuardCharacter.h"

UBTService_SetLookAround::UBTService_SetLookAround()
{
	NodeName = TEXT("Set Look Around");
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

void UBTService_SetLookAround::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	AAIController* AIController = OwnerComp.GetAIOwner();
	AGuardCharacter* GuardCharacter = IsValid(AIController) ? Cast<AGuardCharacter>(AIController->GetPawn()) : nullptr;
	if (IsValid(GuardCharacter))
	{
		GuardCharacter->SetLookAroundType(LookAroundType);
	}
}

void UBTService_SetLookAround::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
	AAIController* AIController = OwnerComp.GetAIOwner();
	AGuardCharacter* GuardCharacter = IsValid(AIController) ? Cast<AGuardCharacter>(AIController->GetPawn()) : nullptr;
	if (IsValid(GuardCharacter))
	{
		GuardCharacter->SetLookAroundType(EGuardLookAroundType::None);
	}
}
