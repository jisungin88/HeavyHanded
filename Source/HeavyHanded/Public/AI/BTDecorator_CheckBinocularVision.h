// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BTDecorator_CheckBinocularVision.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UBTDecorator_CheckBinocularVision : public UBTDecorator_BlackboardBase
{
	GENERATED_BODY()


public:
	UBTDecorator_CheckBinocularVision();

protected:

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	
};
