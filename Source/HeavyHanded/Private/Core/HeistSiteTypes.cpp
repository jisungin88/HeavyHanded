#include "Core/HeistSiteTypes.h"
#include "Core/HeistLog.h"

#if WITH_EDITOR
void FHeistSiteRow::OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName)
{
	const FGameplayTag SiteTag = FGameplayTag::RequestGameplayTag(InRowName, false);
	if (!SiteTag.IsValid())
	{
		UE_LOG(LogHeist, Warning,
			TEXT("[DT_SiteCatalog] 행 이름 '%s'가 등록된 GameplayTag가 아닙니다."
				"행 이름은 Config/Tag/Phase.ini의 Site.* 와 같아야 합니다."), *InRowName.ToString());
	}

	for (const FHeistEntryOption& Option : Entries)
	{
		if (!Option.EntryTag.IsValid())
		{
			UE_LOG(LogHeist, Warning,
				TEXT("[DT_SiteCatalog:%s] 진입점 태그가 비어 있습니다."), *InRowName.ToString());
		}
	}
}
#endif
