#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "NoiseTypes.generated.h"

class UCurveFloat;

// 소음 등급. 계산은 연속값(Loudness01)으로 하고, 등급은 UI 표시와 AI 힌트에만 쓴다.
UENUM(BlueprintType)
enum class ENoiseGrade : uint8
{
	None,
	Small,
	Medium,
	Large,
	Huge
};

// 경계도 4단계. 기획서 3장. 이름은 팀 컨벤션 2-2 예시(EAlertLevel)를 따른다.
UENUM(BlueprintType)
enum class EAlertLevel : uint8
{
	Calm        UMETA(DisplayName = "평온"),        // 0~33%
	Suspicious  UMETA(DisplayName = "의심"),        // 34~66%
	Alerted     UMETA(DisplayName = "경계"),        // 67~99%
	Alarm       UMETA(DisplayName = "경보")         // 100%
};

// DT_NoiseProfiles 한 행. RowName == 게임플레이 태그 이름 (예: Noise.Loot.Throw)
USTRUCT(BlueprintType)
struct FNoiseProfileRow : public FTableRowBase
{
    GENERATED_BODY()

    // 등급 라벨. UI와 AI 힌트 전용
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
    ENoiseGrade Grade = ENoiseGrade::Small;

    // 경계도 증가량. 0.01 == 1%
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", UIMax = "0.5"))
    float AlertDelta = 0.01f;

    // 전파 반경. 소 800 / 중 1500 / 대 3000
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", Units = "cm"))
    float Radius = 800.f;

    // true면 전 구역. 거리 감쇄와 차폐를 모두 무시한다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
    bool bGlobal = false;

    // 이 충격량 이하는 소음 없음 (물리 충돌 전용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Impact", meta = (ClampMin = "0.0"))
    float MinImpulse = 0.f;

    // 이 충격량 이상은 Loudness 1.0으로 포화 (물리 충돌 전용)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Impact", meta = (ClampMin = "0.0"))
    float MaxImpulse = 1000.f;

    // 정규화된 충격량(0~1)을 Loudness로 매핑. 비워두면 선형
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Impact")
    TSoftObjectPtr<UCurveFloat> ImpactCurve;

    // 같은 (에미터, 태그) 조합의 재발행 최소 간격. 충돌 스팸 방지
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", Units = "s"))
    float CooldownSeconds = 0.25f;
};

/**
 * 서버가 발행하는 소음 1건. 감쇄 적용 전.
 * 쿨다운 큐에 잠시 보관되므로 Instigator는 약참조로 둔다 (하드 레퍼런스로 잡으면
 * 파괴된 노획물이 GC되지 않는다). 그래서 BP에는 노출하지 않는다.
 */
USTRUCT(BlueprintType)
struct FNoiseEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FGameplayTag Tag;

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FVector Location = FVector::ZeroVector;

    // 0~1. 충돌 속도 · 재질 · 모디파이어가 모두 반영된 연속값
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    float Loudness01 = 0.f;

    // Loudness로 스케일된 실제 전파 반경 (cm)
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    float Radius = 0.f;

    // 소음을 낸 주체. 결과 화면 "최다 소음 유발자" 집계에 사용
    UPROPERTY()
    TWeakObjectPtr<AActor> InstigatorActor;
};

/**
 * 청취자 1명에게 전달되는 자극. 거리 · 차폐 감쇄 적용 완료.
 * 즉시 소비되고 버려지므로 하드 레퍼런스로 둬서 BP에 노출한다.
 */
USTRUCT(BlueprintType)
struct FNoiseStimulus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FGameplayTag Tag;

    // 청취자가 조사하러 갈 지점
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FVector Location = FVector::ZeroVector;

    // 0~1. 감쇄까지 끝난 최종 강도. 인지 게이지 증가율에 그대로 곱한다
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    float Strength = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    ENoiseGrade Grade = ENoiseGrade::None;

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    TObjectPtr<AActor> InstigatorActor = nullptr;
};
