// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_AttemptArrest.generated.h"

class ABaseCharacter;

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
	float ArrestDuration = 3.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	// 체포 시도 완료까지 기다리는 타이머.
	FTimerHandle ArrestTimerHandle;

	// [체포 추가] 시도 시작 당시 플레이어를 보관한다. 타이머 종료 시 Blackboard에서
	// 새 대상을 가져오면 중간에 변경된 다른 플레이어를 체포할 수 있으므로 같은 대상인지 확인한다.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Arrest")
	TObjectPtr<ABaseCharacter> ArrestTarget;

	// 체포 시도 시간이 끝났을 때 최종 체포 판정을 수행한다.
	// [체포 추가] BT 컴포넌트는 약한 참조로 전달한다. 타이머 전에 BT 소유자가 사라지면 판정을 중단한다.
	void FinishArrest(TWeakObjectPtr<UBehaviorTreeComponent> OwnerCompPtr);

};
