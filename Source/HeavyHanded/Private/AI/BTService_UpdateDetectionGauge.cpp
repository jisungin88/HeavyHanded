#include "AI/BTService_UpdateDetectionGauge.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AITypes.h"
#include "AI/GuardBlackboardKeys.h"
#include "AI/GuardTypes.h"

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

	const bool bCanSeeTarget = BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget);
	const float CurrentGauge = BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge);
	const float Now = AIController->GetWorld()->GetTimeSeconds();

	// 감소 유예 판정은 반드시 LastSeenTime 을 갱신하기 "전에" 한다.
	//
	// 유예가 없으면 시야가 끊기는 순간 게이지가 100 아래로 떨어져
	// Check Detection Gauge 가 먼저 무너지고, 추격 유예(LastSeenTime 기준)가
	// 판정에 개입할 틈이 없다. 유예 동안 게이지를 100 에 붙들어두면
	// 추격 종료 시점을 Check Search Timeout 이 온전히 결정한다.
	const float TimeSinceLastSeen = Now - BlackboardComp->GetValueAsFloat(GuardAIKeys::LastSeenTime);
	const bool bInDecayGrace = !bCanSeeTarget && (TimeSinceLastSeen < DecayGraceSeconds);

	float Delta = 0.f;
	if (bCanSeeTarget)
	{
		// 거리 계수는 상승량에만 곱한다. 코앞이든 시야 끝이든 같은 속도로 발각되면
		// 플레이어가 거리를 두고 움직일 이유가 없어진다.
		Delta = GaugeIncreaseRate * GetDistanceRateMultiplier(*AIController, *BlackboardComp) * DeltaSeconds;
	}
	else if (!bInDecayGrace)
	{
		Delta = -GaugeDecreaseRate * DeltaSeconds;
	}
	// 유예 중에는 Delta = 0 - 게이지를 그대로 유지한다.

	const float NewGauge = FMath::Clamp(CurrentGauge + Delta, 0.f, 100.f);
	BlackboardComp->SetValueAsFloat(GuardAIKeys::DetectionGauge, NewGauge);

	// 보고 있는 동안 "마지막으로 본 시각/위치"를 계속 밀어준다.
	//
	// OnTargetPerceptionUpdated 는 지각 상태가 바뀔 때만 발화하므로, 계속 보고 있어도
	// 이 값들은 획득 순간 한 번만 기록되고 멈춘다. 갱신 주체는 매 틱 도는 이 서비스다.
	//
	// 시야를 잃는 순간 갱신이 멈추면서 값이 그 자리에 얼어붙는다. 추격 브랜치가
	// TargetActor 대신 LastKnownLocation 으로 이동하면, 보이는 동안은 실시간 추적과
	// 같고 놓친 뒤에는 마지막으로 본 자리까지만 가게 된다 - 벽 너머를 꿰뚫어보지 않는다.
	if (bCanSeeTarget)
	{
		BlackboardComp->SetValueAsFloat(GuardAIKeys::LastSeenTime, Now);

		if (const AActor* Target = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor)))
		{
			BlackboardComp->SetValueAsVector(GuardAIKeys::LastKnownLocation, Target->GetActorLocation());
		}
	}

	// 임계값 통과는 상태 전환점이라 한 번씩만 남긴다 (매 틱 로그는 의미가 없다).
	if ((CurrentGauge < 100.f) != (NewGauge < 100.f))
	{
		UE_LOG(LogGuardAI, Log, TEXT("[%s] 인지 게이지 %s (%.1f -> %.1f, 시야=%s)"),
			*GetNameSafe(AIController->GetPawn()),
			NewGauge >= 100.f ? TEXT("가득 참") : TEXT("임계값 아래로"),
			CurrentGauge, NewGauge, bCanSeeTarget ? TEXT("있음") : TEXT("없음"));
	}

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
		const FVector LastKnown = BlackboardComp->GetValueAsVector(GuardAIKeys::LastKnownLocation);
		if (FAISystem::IsValidLocation(LastKnown))
		{
			BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, Now);
			BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, LastKnown);
		}
	}

	// 상태 전환은 더 이상 여기서 하지 않는다.
	// Pursue 브랜치는 CanSeeTarget == true 인 동안 게이지가 100에서 유지되므로 그대로 게이지 판정 사용.
	// Investigate 브랜치는 위에서 기록한 SearchStartTime을 BTDecorator_CheckSearchTimeout이 판정한다.
}

float UBTService_UpdateDetectionGauge::GetDistanceRateMultiplier(
	const AAIController& AIController, const UBlackboardComponent& BlackboardComp) const
{
	const APawn* GuardPawn = AIController.GetPawn();
	const AActor* Target = Cast<AActor>(BlackboardComp.GetValueAsObject(GuardAIKeys::TargetActor));

	if (!IsValid(GuardPawn) || !IsValid(Target))
	{
		// 시야는 잡혔는데 TargetActor 가 아직 안 채워진 틱. 보정 없이 기본 속도로 둔다.
		return 1.f;
	}

	if (FarDistance <= NearDistance)
	{
		// 잘못 설정된 구간. 보간이 성립하지 않으므로 가까운 쪽 계수로 고정한다.
		return NearRateMultiplier;
	}

	const float Distance = FVector::Dist(GuardPawn->GetActorLocation(), Target->GetActorLocation());
	const float Alpha = FMath::Clamp(FMath::GetRangePct(NearDistance, FarDistance, Distance), 0.f, 1.f);

	return FMath::Lerp(NearRateMultiplier, FarRateMultiplier, Alpha);
}
