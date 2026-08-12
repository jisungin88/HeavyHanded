#include "AI/BTDecorator_CheckDetectionGauge.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"

UBTDecorator_CheckDetectionGauge::UBTDecorator_CheckDetectionGauge()
{
	NodeName = TEXT("Check Detection Gauge");
}

bool UBTDecorator_CheckDetectionGauge::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
	{
		return false;
	}

	return BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge) >= GaugeThreshold;
}
