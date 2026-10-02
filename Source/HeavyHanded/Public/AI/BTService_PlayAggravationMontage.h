// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_PlayAggravationMontage.generated.h"

class UAnimMontage;
class UBehaviorTreeComponent;
class UGuardAnimInstance;

UCLASS()
class HEAVYHANDED_API UBTService_PlayAggravationMontage : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_PlayAggravationMontage();

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> AggravationMontage;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.0"))
	float MontageBlendOutTime = 0.15f;

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	UGuardAnimInstance* GetGuardAnimInstance(UBehaviorTreeComponent& OwnerComp) const;
};
