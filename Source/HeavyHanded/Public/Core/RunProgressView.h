#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunProgressView.generated.h"


USTRUCT(BlueprintType)
struct FRunProgressView
{
	GENERATED_BODY()

	/** 팀 공용 잔액 */
	UPROPERTY(BlueprintReadOnly, Category = "Run|Progress")
	int32 TeamGold = 0;

	/** 통과한 장소 */
	UPROPERTY(BlueprintReadOnly, Category = "Run|Progress")
	TArray<FGameplayTag> ClearedSites;

	/** 다음 목표 */
	UPROPERTY(BlueprintReadOnly, Category = "Run|Progress")
	FGameplayTag NextSite;

	/** 캠페인 전체 장소 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Run|Progress")
	int32 SiteNum = 0;

	/** 통과한 장소 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Run|Progress")
	int32 ProgressNum = 0;

	/** 모든 장소 통과 */
	UPROPERTY(BlueprintReadOnly, Category = "Run|Progress")
	bool bCampaignComplete = false;

	/** 체포된 팀원 수 */
	UPROPERTY(BlueprintReadOnly, Category = "Run|Progress")
	int32 ArrestedNum = 0;
};
