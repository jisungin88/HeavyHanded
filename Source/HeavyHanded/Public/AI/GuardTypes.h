#pragma once

#include "CoreMinimal.h"
#include "GuardTypes.generated.h"

// 경비 개체 종류. 동일 BT를 상속하되 서브트리·파라미터 분기에 사용한다.
// GuardAIController::GuardType (UPROPERTY)로만 보관하며, Blackboard에는 복제하지 않는다.
UENUM(BlueprintType)
enum class EGuardType : uint8
{
	Standard UMETA(DisplayName = "일반 경비"),
	Dog      UMETA(DisplayName = "경비견"),
	Armed    UMETA(DisplayName = "무장 경비")
};

