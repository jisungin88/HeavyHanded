#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ENoiseGrade : uint8 { None, Small, Medium, Large, Huge };

UENUM(BlueprintType)
enum class EAlertStage : uint8 { Calm, Suspicious, Alerted, Alarm };

// DataTable Row — RowName == GameplayTag 이름
USTRUCT(BlueprintType)
struct FNoiseProfileRow : public FTableRowBase
{
	ENoiseGrade Grade;
	float AlertDelta;        // 기본 경계도 증가 (0.01 = 1%)
	float Radius;            // cm. 800 / 1500 / 3000
	bool  bGlobal;           // 전 구역 (감쇄 무시)
	float MinImpulse;        // 물리 충돌 전용
	float MaxImpulse;
	TSoftObjectPtr<UCurveFloat> ImpactCurve;
	float CooldownSeconds;   // 스팸 방지
};

// 발행되는 소음 (서버 내부)
USTRUCT(BlueprintType)
struct FNoiseEvent
{
	FGameplayTag Tag;
	FVector Location;
	float Loudness01;        // 0~1 연속값
	float Radius;
	TWeakObjectPtr<AActor> Instigator;
};

// 청취자가 받는 것 (감쇄 적용 후)
USTRUCT(BlueprintType)
struct FNoiseStimulus
{
	FGameplayTag Tag;
	FVector Location;
	float Strength;          // 0~1, 감쇄 적용 완료
	ENoiseGrade Grade;
	TWeakObjectPtr<AActor> Instigator;
};