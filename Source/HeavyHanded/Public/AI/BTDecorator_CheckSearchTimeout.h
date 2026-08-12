#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CheckSearchTimeout.generated.h"

// TimeKeyName(기본: SearchStartTime) 이후 TimeoutSeconds가 지나지 않았으면 true.
// Investigate 브랜치: TimeKeyName=SearchStartTime, TimeoutSeconds=8 (긴 유예 — 조사 지속)
// Pursue 브랜치: TimeKeyName=LastSeenTime, TimeoutSeconds=1~2 (짧은 유예 — 시야 깜빡임 무시)
// 게이지/CanSeeTarget 값과 무관하게 "마지막 기록 시각으로부터 얼마나 지났는지"만으로 판정해,
// 시야 경계에서의 프레임 단위 깜빡임이 Selector 브랜치를 매 틱 뒤집는 것을 방지한다.
UCLASS()
class UBTDecorator_CheckSearchTimeout : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckSearchTimeout();

	UPROPERTY(EditAnywhere, Category = "Condition")
	FName TimeKeyName = TEXT("SearchStartTime");

	UPROPERTY(EditAnywhere, Category = "Condition")
	float TimeoutSeconds = 8.f;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
