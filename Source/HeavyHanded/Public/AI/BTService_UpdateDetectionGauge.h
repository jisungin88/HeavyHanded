#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateDetectionGauge.generated.h"

class AAIController;
class UBlackboardComponent;

// 시야/청각 조건에 따라 DetectionGauge(0~100)를 매 틱 갱신한다.
// 게이지가 가득 차면 AIState를 Investigate 또는 Pursue로 전환한다.
UCLASS()
class UBTService_UpdateDetectionGauge : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateDetectionGauge();

	UPROPERTY(EditAnywhere, Category = "Detection")
	float GaugeIncreaseRate = 40.f; // 초당 상승량 (CanSeeTarget == true일 때, 거리 계수 적용 전)

	UPROPERTY(EditAnywhere, Category = "Detection")
	float GaugeDecreaseRate = 15.f; // 초당 감소량 (반경 이탈 시). 거리 무관.

	// 시야를 잃어도 이 시간 동안은 게이지를 유지한다(감소 시작을 미룬다).
	//
	// 없으면 시야가 끊기는 즉시 게이지가 임계값 아래로 떨어져 Check Detection Gauge 가
	// 먼저 무너지고, 추격 유예(Check Search Timeout, LastSeenTime 기준)가 판정에
	// 개입할 틈이 없다. 추격 유예와 같거나 조금 크게 두는 것이 자연스럽다.
	UPROPERTY(EditAnywhere, Category = "Detection", meta = (ClampMin = "0.0", Units = "s"))
	float DecayGraceSeconds = 4.f;

	// --- 거리에 따른 포착 속도 ---
	// 상승량에만 곱해진다. NearDistance 이내는 NearRateMultiplier 고정,
	// FarDistance 이상은 FarRateMultiplier 고정, 그 사이는 선형 보간.
	//
	// 기본값 기준 포착 시간(GaugeIncreaseRate = 40):
	//   300cm 이내 - 40 x 2.5 = 100/초  -> 1.0초
	//   1650cm     - 40 x 1.5 =  60/초  -> 1.7초
	//   3000cm 이상- 40 x 0.5 =  20/초  -> 5.0초

	UPROPERTY(EditAnywhere, Category = "Detection|Distance", meta = (ClampMin = "0.0", Units = "cm"))
	float NearDistance = 300.f;

	// 시야 반경(SightConfig 의 Sight Radius)과 맞춰두는 것이 자연스럽다.
	UPROPERTY(EditAnywhere, Category = "Detection|Distance", meta = (ClampMin = "0.0", Units = "cm"))
	float FarDistance = 3000.f;

	UPROPERTY(EditAnywhere, Category = "Detection|Distance", meta = (ClampMin = "0.0"))
	float NearRateMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Detection|Distance", meta = (ClampMin = "0.0"))
	float FarRateMultiplier = 0.5f;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// 경비와 TargetActor 사이 거리로 상승량 계수를 구한다.
	// 타겟을 못 찾으면 1.0(거리 보정 없음)을 돌려준다.
	float GetDistanceRateMultiplier(const AAIController& AIController, const UBlackboardComponent& BlackboardComp) const;
};
