#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DeveloperSettings.h"
#include "NoiseSettings.generated.h"

class UDataTable;

/**
 * 소음 시스템 프로젝트 세팅. Project Settings → Game → Noise.
 * 밸런싱 수치(등급/경계도/반경)는 DT_NoiseProfiles
 * 여기에는 그 테이블을 가리키는 경로와, 기획자가 만질 일 없는 테크 튜닝 값만 둔다.
 */
UCLASS(config = NoiseSystem, defaultconfig, meta = (DisplayName = "Noise"))
class HEAVYHANDED_API UNoiseSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UNoiseSettings()
	{
		// 재질별 소음 계수. 두 재질 중 작은 쪽이 이긴다 (흡음 우선)
		SurfaceNoiseCoeff.Add(SurfaceType_Default, 1.00f);
		SurfaceNoiseCoeff.Add(SurfaceType1,        1.00f);  // Concrete
		SurfaceNoiseCoeff.Add(SurfaceType2,        0.80f);  // Wood
		SurfaceNoiseCoeff.Add(SurfaceType3,        1.00f);  // Metal
		SurfaceNoiseCoeff.Add(SurfaceType4,        1.00f);  // Glass
		SurfaceNoiseCoeff.Add(SurfaceType5,        0.35f);  // Carpet
		SurfaceNoiseCoeff.Add(SurfaceType6,        0.90f);  // Tile
		SurfaceNoiseCoeff.Add(SurfaceType7,        0.50f);  // Dirt

		// 차폐물 재질별 감쇄율. 작을수록 잘 막는다
		OccluderFactor.Add(SurfaceType_Default,    0.50f);
		OccluderFactor.Add(SurfaceType1,           0.25f);  // Concrete
		OccluderFactor.Add(SurfaceType2,           0.45f);  // Wood
		OccluderFactor.Add(SurfaceType3,           0.30f);  // Metal
		OccluderFactor.Add(SurfaceType4,           0.80f);  // Glass
		OccluderFactor.Add(SurfaceType5,           0.50f);  // Carpet
		OccluderFactor.Add(SurfaceType6,           0.40f);  // Tile
		OccluderFactor.Add(SurfaceType7,           0.35f);  // Dirt
	}
	
	/**
	 * CDO 라 절대 null 이 아니다 — **null 검사를 하지 말 것.**
	 * `Settings ? Settings->X : 리터럴` 은 죽은 분기를 만들고, 그 리터럴이 기본값과 따로 논다.
	 */
	static const UNoiseSettings* Get() { return GetDefault<UNoiseSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ── 데이터 ──

	/** 행동별 등급/경계도/반경. RowName == 게임플레이 태그 이름 */
	UPROPERTY(config, EditAnywhere, Category = "Data",
			meta = (AllowedClasses = "/Script/Engine.DataTable",
							RequiredAssetDataTags = "RowStructure=/Script/HeavyHanded.NoiseProfileRow"))
	TSoftObjectPtr<UDataTable> NoiseProfiles;

	/** 재질별 소음 계수. 카펫처럼 작을수록 조용하다 */
	UPROPERTY(config, EditAnywhere, Category = "Data", meta = (ClampMin = "0.0"))
	TMap<TEnumAsByte<EPhysicalSurface>, float> SurfaceNoiseCoeff;

	/** 차폐물 재질별 감쇄율. 작을수록 잘 막는다 */
	UPROPERTY(config, EditAnywhere, Category = "Data", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	TMap<TEnumAsByte<EPhysicalSurface>, float> OccluderFactor;

	// ── 감쇄 ──

	/** 거리 감쇄 지수. 1이면 선형, 클수록 소음원 근처에서 급감 */
	UPROPERTY(config, EditAnywhere, Category = "Attenuation", meta = (ClampMin = "0.1", UIMax = "4.0"))
	float DistanceFalloffExponent = 1.7f;

	/** 이 강도 미만이면 안 들린 것으로 버린다 */
	UPROPERTY(config, EditAnywhere, Category = "Attenuation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinAudibleStrength = 0.05f;

	/** 오클루전 트레이스에서 세는 차폐물 최대 개수 */
	UPROPERTY(config, EditAnywhere, Category = "Attenuation", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxOccluders = 4;

	// ── 디버그 ──

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0", Units = "s"))
	float DebugDrawDuration = 2.f;

	// ── 조회 (누락 키를 0으로 떨어뜨리지 않기 위한 래퍼) ──

	float GetSurfaceCoeff(EPhysicalSurface Surface) const
	{
		const float* Found = SurfaceNoiseCoeff.Find(Surface);
		return Found ? *Found : 1.0f;
	}

	float GetOccluderFactor(EPhysicalSurface Surface) const
	{
		const float* Found = OccluderFactor.Find(Surface);
		return Found ? *Found : 0.5f;
	}
};
