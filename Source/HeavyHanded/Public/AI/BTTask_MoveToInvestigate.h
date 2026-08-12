#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MoveToInvestigate.generated.h"

// InvestigateLocation(소음/의심 지점)으로 이동한다.
//
// 래턴트 태스크다 - 이동 명령만 넣고 즉시 Succeeded 를 반환하면 Sequence 가
// 매 틱 완료되어 Selector 가 이 브랜치를 계속 재실행하고, 이동 명령도 매 틱
// 새로 발행된다. 도착(또는 이동 실패)까지 InProgress 를 유지해야 한다.
//
// 수색 타임아웃의 기준 시각(SearchStartTime)은 여기서 건드리지 않는다.
// 시야 상실과 소음 감지 시점에 GuardAIController 가 이미 기록하며,
// 이 태스크가 실행할 때마다 덮어쓰면 타임아웃이 영원히 만료되지 않는다.
UCLASS()
class UBTTask_MoveToInvestigate : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MoveToInvestigate();

	UPROPERTY(EditAnywhere, Category = "Investigate")
	float AcceptanceRadius = 60.f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
