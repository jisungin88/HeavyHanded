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

// 순찰 지점 배열을 도는 방식.
UENUM(BlueprintType)
enum class EPatrolPattern : uint8
{
	Loop     UMETA(DisplayName = "순환 (0→1→2→3→0→1...)"),
	PingPong UMETA(DisplayName = "왕복 (0→1→2→3→2→1→0...) - 기본 추천"),
	Random   UMETA(DisplayName = "무작위 (직전 지점 제외 랜덤)")
};

