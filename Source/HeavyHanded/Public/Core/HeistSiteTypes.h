#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "HeistSiteTypes.generated.h"

class UWorld;
class UTexture2D;

USTRUCT(BlueprintType)
struct FHeistEntryOption
{
	GENERATED_BODY()

	/** 진입점 식별자 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry", meta = (Categories = "Entry"))
	FGameplayTag EntryTag;

	/** 진입점 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry")
	FText DisplayName;

	/** 진입점 설명 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry", meta = (MultiLine = "true"))
	FText Description;

	/** 진입점 아이콘 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> Image;
};

USTRUCT(BlueprintType)
struct FHeistSiteRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 스테이지 순서 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Campaign", meta = (ClampMin = "1"))
	int32 CampaignOrder = 1;

	/** 스테이지 레벨 */
	UPROPERTY(EditAnywhere, Category = "Travel", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> Level;

	/** 목표 금액 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0"))
	int32 TargetValue = 0;

	/** 작업 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1.0", Units = "s"))
	float HeistSeconds = 420.f;

	/** 탈출 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0.0", Units = "s"))
	float EscapeSeconds = 90.f;

	/** 스테이지 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FText DisplayName;

	/** 스테이지 설명 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (MultiLine = "true"))
	FText Description;

	/** 스테이지 아이콘 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> Image;

	/** 스테이지 진입점 */
	UPROPERTY(EditAnywhere, Category = "Entries", meta = (TitleProperty = "EntryTag"))
	TArray<FHeistEntryOption> Entries;

#if WITH_EDITOR
	/** 표를 저장할 때 행 이름(Site.), 진입점(Entry.) 확인 */
	virtual void OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName) override;
#endif
};
