#include "AI/BTService_UpdateDetectionGauge.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_UpdateDetectionGauge::UBTService_UpdateDetectionGauge()
{
	NodeName = TEXT("Update Detection Gauge");
	Interval = 0.1f; // 0.1초마다 갱신 (매 프레임 갱신은 과함)
}

void UBTService_UpdateDetectionGauge::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	// 인지 판정 자체는 서버에서만 의미 있는 상태 전환을 유발해야 한다.
	const AAIController* AIController = OwnerComp.GetAIOwner();
	if (!IsValid(AIController) || !AIController->HasAuthority())
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!IsValid(BlackboardComp))
	{
		return;
	}

	const bool bCanSeeTarget = BlackboardComp->GetValueAsBool(TEXT("CanSeeTarget"));
	const float CurrentGauge = BlackboardComp->GetValueAsFloat(TEXT("DetectionGauge"));

	const float Delta = bCanSeeTarget ? (GaugeIncreaseRate * DeltaSeconds) : (-GaugeDecreaseRate * DeltaSeconds);
	const float NewGauge = FMath::Clamp(CurrentGauge + Delta, 0.f, 100.f);
	BlackboardComp->SetValueAsFloat(TEXT("DetectionGauge"), NewGauge);

	// 상태 전환은 더 이상 여기서 하지 않는다.
	// DetectionGauge >= 100 && CanSeeTarget 조건은 BTDecorator_CheckDetectionGauge가 Pursue 브랜치에서,
	// DetectionGauge >= 100 && !CanSeeTarget 조건은 같은 Decorator를 Investigate 브랜치에서 각각 판정한다.
}
