// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_SetLookAround.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UBTService_SetLookAround : public UBTService
{
	GENERATED_BODY()

public:

	UBTService_SetLookAround();

protected:

	// Wait 노드가 실행되기 시작하면 주변을 두리번거리는 애니메이션을 활성화한다.
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Wait 노드가 종료되거나 Abort되면 주변을 두리번거리는 애니메이션을 비활성화한다.
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
};
