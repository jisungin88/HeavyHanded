#include "AI/BTDecorator_CheckWorldAlert.h"
#include "AIController.h"
#include "AI/GuardAIController.h"

UBTDecorator_CheckWorldAlert::UBTDecorator_CheckWorldAlert()
{
	NodeName = TEXT("Check World Alert");
}

bool UBTDecorator_CheckWorldAlert::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AGuardAIController* GuardController = Cast<AGuardAIController>(OwnerComp.GetAIOwner());
	if (!IsValid(GuardController))
	{
		return false;
	}

	// 읽기 전용 조회. 값 변경은 서버 권한 로직(소음 판정 함수) 쪽에서만 일어난다.
	return GuardController->GetWorldAlertLevel() >= AlertThreshold;
}
