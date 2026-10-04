// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_PlayAggravationMontage.generated.h"

class UAnimMontage;
class UBehaviorTreeComponent;
class AGuardCharacter;

UCLASS()
class HEAVYHANDED_API UBTService_PlayAggravationMontage : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_PlayAggravationMontage();

	// 캐릭터 BP 미지정 시 사람 경비가 사용하는 기존 BT 설정. 경비견에는 적용하지 않는다.
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimMontage> AggravationMontage;

	UPROPERTY(EditAnywhere, Category = "Animation", meta = (ClampMin = "0.0"))
	float MontageBlendOutTime = 0.15f;

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	AGuardCharacter* GetGuardCharacter(UBehaviorTreeComponent& OwnerComp) const;

	// 재생 때 선택한 몽타주를 보관해 종료 때도 같은 몽타주만 중단한다.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ActiveMontage;
};
