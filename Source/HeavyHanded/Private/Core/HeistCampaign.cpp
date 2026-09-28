#include "Core/HeistCampaign.h"

namespace HeistCampaign
{
	FGameplayTag GetNextSite(const TArray<FGameplayTag>& Order, const TArray<FGameplayTag>& Cleared)
	{
		for (const FGameplayTag& Site : Order)
		{
			if (!Site.IsValid())
			{
				continue;
			}

			if (!Cleared.Contains(Site))
			{
				return Site;
			}
		}

		return FGameplayTag();
	}

	int32 GetProgress(const TArray<FGameplayTag>& Order, const TArray<FGameplayTag>& Cleared)
	{
		int32 Progress = 0;
		for (const FGameplayTag& Site : Order)
		{
			if (Site.IsValid() && Cleared.Contains(Site))
			{
				++Progress;
			}
		}

		return Progress;
	}

	bool IsComplete(const TArray<FGameplayTag>& Order, const TArray<FGameplayTag>& Cleared)
	{
		int32 ValidNum = 0;
		for (const FGameplayTag& Site : Order)
		{
			if (Site.IsValid())
			{
				++ValidNum;
			}
		}

		if (ValidNum == 0)
		{
			return false;
		}

		return GetProgress(Order, Cleared) >= ValidNum;
	}
}
