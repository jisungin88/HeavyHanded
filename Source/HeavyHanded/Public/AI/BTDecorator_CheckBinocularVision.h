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

	// 바라보기 분기에만 켠다. 주변 시야에서 재발견해도 게이지가 찰 때까지 대상을 관찰한다.
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bAllowPeripheralObservation = false;

	virtual FString GetStaticDescription() const override;

protected:

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	
};
