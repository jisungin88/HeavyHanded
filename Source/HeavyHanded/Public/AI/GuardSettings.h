#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GuardSettings.generated.h"

class UDataTable;

/**
 * 경비 AI 프로젝트 세팅. Project Settings → Game → Guard.
 * 밸런싱 수치(이동/지각/조사)는 DT_GuardStats(FGuardStatsRow) 에 두고,
 * 여기에는 그 테이블을 가리키는 경로만 둔다. NoiseSettings 와 동일한 패턴.
 */
UCLASS(config = GuardSystem, defaultconfig, meta = (DisplayName = "Guard"))
class HEAVYHANDED_API UGuardSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * CDO 라 절대 null 이 아니다 — 호출부에서 null 검사를 하지 말 것.
	 */
	static const UGuardSettings* Get() { return GetDefault<UGuardSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 경비 종류별(EGuardType) 이동/지각/조사 수치. RowName == EGuardType 이름 문자열 */
	UPROPERTY(config, EditAnywhere, Category = "Data",
			meta = (AllowedClasses = "/Script/Engine.DataTable",
							RequiredAssetDataTags = "RowStructure=/Script/HeavyHanded.GuardStatsRow"))
	TSoftObjectPtr<UDataTable> GuardStats;
};
