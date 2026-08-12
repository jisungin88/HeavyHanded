#include "AI/BTDecorator_CheckDetectionGauge.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "AI/GuardBlackboardKeys.h"

UBTDecorator_CheckDetectionGauge::UBTDecorator_CheckDetectionGauge()
{
	NodeName = TEXT("Check Detection Gauge");

	// 고를 수 있는 키를 Float 로 제한하고 기본값을 DetectionGauge 로 둔다.
	BlackboardKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_CheckDetectionGauge, BlackboardKey));
	BlackboardKey.SelectedKeyName = GuardAIKeys::DetectionGauge;
}

bool UBTDecorator_CheckDetectionGauge::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
	{
		return false;
	}

	return BlackboardComp->GetValueAsFloat(BlackboardKey.SelectedKeyName) >= GaugeThreshold;
}
