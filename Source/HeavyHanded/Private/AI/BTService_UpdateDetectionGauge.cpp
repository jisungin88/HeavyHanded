#include "AI/BTService_UpdateDetectionGauge.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AITypes.h"

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

	// 게이지가 이번 틱에 막 100을 넘겼고(직전 틱엔 100 미만), 타겟이 안 보이는 상황이면
	// "조사 시작 시각"을 기록한다. 이후엔 게이지 값과 무관하게 이 시각 기준으로만
	// Investigate 지속 여부를 판정한다 (BTDecorator_CheckSearchTimeout이 담당).
	// LastKnownLocation이 실제로 유효할 때만 진입한다 - 갈 곳 없이 타이머만 켜지는 것을 방지.
	// 안 보이게 된 "그 순간"을 감지하려 하지 않는다 - 게이지는 CanSeeTarget==true일 때만
	// 증가하므로 "!bCanSeeTarget && 방금 100 넘음"이라는 조합은 애초에 성립할 수 없다.
	// 대신 "보고 있고 게이지가 100 이상인 동안" 매 틱 SearchStartTime/InvestigateLocation을
	// 최신 상태로 계속 갱신해둔다. 그러면 시야를 잃는 바로 그 틱에 자연스럽게 값이 얼어붙어
	// "마지막으로 확실히 본 시점/위치"로 남는다.
	if (bCanSeeTarget && NewGauge >= 100.f)
	{
		const FVector LastKnown = BlackboardComp->GetValueAsVector(TEXT("LastKnownLocation"));
		if (FAISystem::IsValidLocation(LastKnown))
		{
			BlackboardComp->SetValueAsFloat(TEXT("SearchStartTime"), AIController->GetWorld()->GetTimeSeconds());
			BlackboardComp->SetValueAsVector(TEXT("InvestigateLocation"), LastKnown);
		}
	}

	// 상태 전환은 더 이상 여기서 하지 않는다.
	// Pursue 브랜치는 CanSeeTarget == true 인 동안 게이지가 100에서 유지되므로 그대로 게이지 판정 사용.
	// Investigate 브랜치는 위에서 기록한 SearchStartTime을 BTDecorator_CheckSearchTimeout이 판정한다.
}
