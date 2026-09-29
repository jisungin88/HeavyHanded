// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/GuardTypes.h"
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

	// 이 서비스가 활성화된 Wait에서 재생할 두리번 애니메이션 종류.
	UPROPERTY(EditAnywhere, Category = "Animation")
	EGuardLookAroundType LookAroundType = EGuardLookAroundType::None;

protected:

	// Wait 노드가 실행되기 시작하면 주변을 두리번거리는 애니메이션을 활성화한다.
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// Wait 노드가 종료되거나 Abort되면 주변을 두리번거리는 애니메이션을 비활성화한다.
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
};
