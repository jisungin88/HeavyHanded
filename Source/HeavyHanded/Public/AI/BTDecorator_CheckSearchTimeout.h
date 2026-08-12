#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CheckSearchTimeout.generated.h"

// TimeKeyName(기본: SearchStartTime) 이후 TimeoutSeconds가 지나지 않았으면 true.
// 게이지/CanSeeTarget 값과 무관하게 "마지막 기록 시각으로부터 얼마나 지났는지"만으로 판정해,
// 시야 경계에서의 프레임 단위 깜빡임이 Selector 브랜치를 매 틱 뒤집는 것을 방지한다.
//
// 브랜치별 권장값 (에셋에서 노드마다 설정한다 - 아래 기본값은 조사 쪽 기준):
//   Investigate : TimeKeyName=SearchStartTime, TimeoutSeconds=12
//                 한 지점만 찍고 끝나는 구조라 짧으면 너무 쉽게 따돌려진다.
//   Pursue      : TimeKeyName=LastSeenTime,    TimeoutSeconds=4
//                 모퉁이 하나 도는 동안은 놓치지 않을 만큼. LastSeenTime 은
//                 BTService_UpdateDetectionGauge 가 보고 있는 동안 계속 갱신한다.
UCLASS()
class UBTDecorator_CheckSearchTimeout : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckSearchTimeout();

	UPROPERTY(EditAnywhere, Category = "Condition")
	FName TimeKeyName = TEXT("SearchStartTime");

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 12.f;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
