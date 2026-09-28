// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_AttemptArrest.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UBTTask_AttemptArrest : public UBTTask_BlackboardBase
{
	GENERATED_BODY()


public:
	UBTTask_AttemptArrest();

	// 체포 시도에 필요한 최소 거리.
	UPROPERTY(EditAnywhere, Category = "Arrest", meta = (ClampMin = "0.0"))
	float ArrestRange = 150.0f;

	// 체포 판정을 완료하기까지 걸리는 시간.
	UPROPERTY(EditAnywhere, Category = "Arrest", meta = (ClampMin = "0.0"))
	float ArrestDuration = 2.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	// 체포 시도 완료까지 기다리는 타이머.
	FTimerHandle ArrestTimerHandle;

	// 체포 시도 시간이 끝났을 때 최종 체포 판정을 수행한다.
	void FinishArrest(UBehaviorTreeComponent* OwnerComp);

};
