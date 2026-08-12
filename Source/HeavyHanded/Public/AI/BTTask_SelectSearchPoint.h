#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SelectSearchPoint.generated.h"

// 다음 수색 지점을 골라 Blackboard의 InvestigateLocation에 써넣는다.
// 실제 선택 로직은 AGuardAIController::SelectNextSearchPoint 에 있다.
//
// 한 번의 조사는 [마지막 목격 지점] -> [주변 무작위 지점 x SearchSweepCount] 로
// 진행되고, 다 훑으면 Failed 를 돌려준다. 조사 브랜치의 Sequence 가 그때 실패하면서
// Selector 가 순찰로 내려간다.
//
// 조사 브랜치 구성 예:
//   Sequence [CanSeeTarget Is Not Set / Check Search Timeout]
//     - Select Search Point   <- 이 노드
//     - Move To Investigate
//     - Wait (두리번거리는 시간)
UCLASS()
class UBTTask_SelectSearchPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SelectSearchPoint();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
