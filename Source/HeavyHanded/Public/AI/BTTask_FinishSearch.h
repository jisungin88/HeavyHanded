#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FinishSearch.generated.h"

// 수색 제한이 끝난 세션에 대해 마지막 목격 위치 확인과 대기를 한 번 수행한다.
// 최상위 Selector에서 일반 수색보다 뒤, 순찰보다 앞에 연결한다.
UCLASS()
class HEAVYHANDED_API UBTTask_FinishSearch : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FinishSearch();

	// 일반 수색의 Check Search Timeout과 같은 값으로 설정한다.
	UPROPERTY(EditAnywhere, Category = "Search|Finish", meta = (ClampMin = "0.0", Units = "s"))
	float SearchTimeoutSeconds = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Search|Finish", meta = (ClampMin = "0.0", Units = "s"))
	float WaitSeconds = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Search|Finish", meta = (ClampMin = "0.0", Units = "cm"))
	float AcceptanceRadius = 60.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	float ActiveSearchStartTime = -100000.0f;
	float CompletedSearchStartTime = -100000.0f;
	float WaitStartTime = 0.0f;
	bool bWaiting = false;
};
