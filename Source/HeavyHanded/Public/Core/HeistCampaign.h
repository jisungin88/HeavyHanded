#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"


namespace HeistCampaign
{
	HEAVYHANDED_API FGameplayTag GetNextSite(
		const TArray<FGameplayTag>& Order, const TArray<FGameplayTag>& Cleared);

	HEAVYHANDED_API int32 GetProgress(
		const TArray<FGameplayTag>& Order, const TArray<FGameplayTag>& Cleared);

	HEAVYHANDED_API bool IsComplete(
		const TArray<FGameplayTag>& Order, const TArray<FGameplayTag>& Cleared);
}
