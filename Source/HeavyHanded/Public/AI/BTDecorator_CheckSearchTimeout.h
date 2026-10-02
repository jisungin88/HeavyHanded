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
//   >> 조사를 시작한 후 12초 동안은 Investigate Branch를 유지한다. // 조사 자체에 시간 제한
// 
//   Pursue      : TimeKeyName=LastSeenTime,    TimeoutSeconds=4
//                 모퉁이 하나 도는 동안은 놓치지 않을 만큼. LastSeenTime 은
//                 BTService_UpdateDetectionGauge 가 보고 있는 동안 계속 갱신한다.
//   >> 마지막으로 본 순간부터 4초 정도는 계속 추격 // 시야를 잃었을 때 추격을 얼마나 오래 유지할지
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

	// SearchStartTime 수색에만 적용한다. 간격과 실패 소요가 모두 이 값보다 짧아야 한다. 0이면 비활성화.
	UPROPERTY(EditAnywhere, Category = "Condition|Retry", meta = (ClampMin = "0.0", Units = "s", DisplayName = "빠른 재시도 판단 시간"))
	float QuickRetryThresholdSeconds = 1.f;

	// 빠른 실패가 2회 연속일 때 원래 제한시간에 한 번만 더한다. 0이면 연장하지 않는다.
	UPROPERTY(EditAnywhere, Category = "Condition|Retry", meta = (ClampMin = "0.0", Units = "s", DisplayName = "추가 탐색 시간"))
	float QuickRetryExtensionSeconds = 3.f;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
