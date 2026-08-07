#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateDetectionGauge.generated.h"

// 시야/청각 조건에 따라 DetectionGauge(0~100)를 매 틱 갱신한다.
// 게이지가 가득 차면 AIState를 Investigate 또는 Pursue로 전환한다.
UCLASS()
class UBTService_UpdateDetectionGauge : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateDetectionGauge();

	UPROPERTY(EditAnywhere, Category = "Detection")
	float GaugeIncreaseRate = 40.f; // 초당 상승량 (CanSeeTarget == true일 때)

	UPROPERTY(EditAnywhere, Category = "Detection")
	float GaugeDecreaseRate = 15.f; // 초당 감소량 (반경 이탈 시)

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
