#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GuardCharacter.generated.h"

UCLASS()
class AGuardCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// 레벨에 배치된 경비 인스턴스마다 서로 다른 순찰 경로를 지정할 수 있도록
	// EditInstanceOnly로 노출한다 (블루프린트 기본값이 아니라, 레벨의 각 배치본에서 직접 설정).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Guard|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	// 유효 범위를 벗어나면 첫 지점으로 순환한다. 배열이 비어있으면 false를 반환.
	UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	bool GetPatrolLocation(int32 Index, FVector& OutLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	int32 GetPatrolPointCount() const { return PatrolPoints.Num(); }
};
