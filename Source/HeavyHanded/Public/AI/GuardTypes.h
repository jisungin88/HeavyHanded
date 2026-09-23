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

// 현재 안쓰는 중
/*
USTRUCT(BlueprintType)
struct FGuardHearingConfig
{
	GENERATED_BODY()

	// AI Hearing 감지 반경
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0.0", Units = "cm"))
	float HearingRangeNew = 1200.f;

	// UPerceptionMeterComponent
	// 
	// 귀 높이. Owner 가 폰이 아닐 때만 쓰는 오프셋.
	// ClampMax 는 UNoiseSubsystem 의 ListenerCullMargin 과 묶여 있다 — 그보다 크게 열면
	// 반경 경계의 청취자가 1차 거리 컬링에서 조용히 걸러진다.
	// 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing", meta = (ClampMin = "0.0", ClampMax = "300.0", Units = "cm"))
	float EarHeightNew = 60.f;


	// UPerceptionMeterComponent
	// 게이지가 다 차야 반응하는 최대치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hearing|Perception", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PerceptionFullThresholdNew = 0.5f;

};
*/ 


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

	// 수평 시야
	// 경비병이 전체적으로 볼 수 있는 수평 시야각. ex) 180도라면 정면 기준 좌우 각각 90도까지 본다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "60.0", ClampMax = "240", Units = "deg"))
	float PeripheralVisionAngleDegrees = 180.f;

	// 양안 시야각
	// 두 눈이 동시에 대상을 바라보는 중앙 영역의 전체 각도. ex) 60도라면 정면 기준 좌우 각각 30도씩이다.
	// AI Perception의 실제 시야 범위를 줄이는 값이 아니라, 이후 인지 게이지 상승 속도를 보정하는 데 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float BinocularVisionAngleDegrees = 60.f;


	// 수직 시야
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "20.0", ClampMax = "60.0", Units = "deg"))
	float VerticalVisionAngleDegrees = 45.0f;


	//구조체로
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

	// 무자극 상태에서 인지 게이지가 초당 얼마나 식는지 (UPerceptionMeterComponent::DecayPerSecond 를 덮어쓴다).
	// 기본값은 그 컴포넌트의 기본값과 같다 - 경비견처럼 한 번 물면 잘 안 놓는 타입만 낮춰서 차별화한다.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.0"))
	float PerceptionDecayPerSecond = 0.2f;
};



// 경비의 월드 경계도 반응에 필요한 설정 및 런타임 상태.
// 월드 경계도 자체는 UAlertComponent가 관리하고,
// 이 구조체는 경비가 월드 경계도에 반응하는 데 필요한 값만 관리한다.
USTRUCT(BlueprintType)
struct FGuardWorldAlertSettings
{
	GENERATED_BODY()

	// 월드 경계도가 이 값 이상이면 경비가 추적 속도로 이동한다.
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Movement")
	float WorldAlertSpeedThreshold = 34.0f;

	// 월드 경계도가 임계값 이상일 때 기본 이동 속도에 적용할 증가율.
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Movement")
	float WorldAlertMoveSpeedMultiplier = 1.3f;

	// 월드 경계도가 속도 증가 임계값 이상일 때 새로운 소음이 발생하지 않아야 하는 시간.
	// 이 시간이 지나면 경비의 속도 증가 상태를 해제한다.
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Movement")
	float WorldAlertSilenceDelay = 20.0f;


	// GuardStats DataTable에서 적용한 기본 이동 속도.
	// 월드 경계도가 다시 내려가면 이 속도로 복구한다.
	float NormalMoveSpeed = 0.0f;

	// 현재 월드 경계도에 의해 가속된 상태인지 여부.
	// 상태가 실제로 변경될 때만 이동 속도를 갱신하기 위해 사용한다.
	bool bWorldAlertSpeedUp = false;

	// 현재 경계도 임계값 구간에서 이미 속도 증가를 발동했는지 여부.
	bool bWorldAlertSpeedTriggered = false;

	// 게이지가 올라갈 때만 리셋 위함.
	float PreviousWorldAlertLevel = 0.0f;

};

