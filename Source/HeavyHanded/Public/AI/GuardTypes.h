#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GuardTypes.generated.h"

// 경비 AI 공통 로그 카테고리. 정의는 GuardAIController.cpp.
// 이 경로는 실패해도 예외가 나지 않고 "가만히 서 있는다"로만 드러나므로,
// 조용히 return 하는 지점마다 이유를 남긴다.
DECLARE_LOG_CATEGORY_EXTERN(LogGuardAI, Log, All);

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

// DT_GuardStats 한 행. RowName == EGuardType 이름 문자열 (예: "Standard", "Dog", "Armed").
// GuardAIController::OnPossess 가 GuardType 으로 이 행을 찾아 이동/지각/조사 수치를
// 일괄 적용한다 — BP 인스턴스 기본값은 테이블 조회가 실패했을 때만 쓰는 폴백이다.
USTRUCT(BlueprintType)
struct FGuardStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	// ── 이동 ──

	// 단위: cm/s (UE 에는 cm/s 단위 지정자가 없어 Units 메타 없이 클램프만 건다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 300.f;

	// ── 지각 (AISenseConfig_Sight/Hearing 에 그대로 적용) ──

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.0", Units = "cm"))
	float SightRadius = 1500.f;

	// 시야 반경보다 커야 한다 - 한 번 본 대상을 더 먼 거리까지 계속 추적하기 위한 값
	// (AISenseConfig_Sight::LoseSightRadius 의 관례).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.0", Units = "cm"))
	float LoseSightRadius = 1700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float PeripheralVisionAngleDegrees = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.0", Units = "cm"))
	float HearingRange = 1200.f;

	// ── 순찰 ──

	// 이 거리(2D) 안이면 현재 순찰 지점에 도착한 것으로 본다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Patrol", meta = (ClampMin = "0.0", Units = "cm"))
	float PatrolArrivalRadius = 120.f;

	// ── 조사 ──

	// 마지막 목격/소음 지점을 확인한 뒤 주변을 몇 번 더 훑을지. 0 이면 지점만 확인하고 순찰로 복귀.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0"))
	int32 SearchSweepCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0.0", Units = "cm"))
	float SearchSweepRadius = 600.f;

	// 머리 위 게이지 위젯 갱신 주기. BTService_UpdateDetectionGauge 와 같은 값으로 맞출 것.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.01", Units = "s"))
	float HeadGaugeUpdateInterval = 0.1f;
};

