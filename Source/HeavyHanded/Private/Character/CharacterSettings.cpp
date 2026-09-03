#include "Character/CharacterSettings.h"

#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterSettings, Log, All);

UClass* UCharacterSettings::ResolveRolePawnClass(const FGameplayTag& RoleTag) const
{
	if (RoleTag.IsValid())
	{
		for (const FHeistRolePawn& Entry : RolePawnClasses)
		{
			if (Entry.RoleTag != RoleTag)
			{
				continue;
			}

			if (Entry.PawnClass.IsNull())
			{
				UE_LOG(LogCharacterSettings, Warning,
									  TEXT("역할 %s 의 PawnClass 가 비어 있습니다. 기본 폰으로 떨어집니다 — "
											   "Project Settings → Game → Character 에서 지정하세요."),
									  *RoleTag.ToString());
				break;
			}

			if (UClass* Loaded = Entry.PawnClass.LoadSynchronous())
			{
				return Loaded;
			}

			UE_LOG(LogCharacterSettings, Warning,
							  TEXT("역할 %s 의 폰 클래스 '%s' 를 로드하지 못했습니다. 기본 폰으로 떨어집니다."),
							  *RoleTag.ToString(), *Entry.PawnClass.ToString());
			break;

		}
	}

	if (!DefaultRolePawnClass.IsNull())
	{
		if (UClass* Loaded = DefaultRolePawnClass.LoadSynchronous())
		{
			if (RoleTag.IsValid())
			{
				UE_LOG(LogCharacterSettings, Warning,
									  TEXT("역할 %s 에 매핑된 폰이 없어 기본 폰(%s)으로 스폰합니다."),
									  *RoleTag.ToString(), *Loaded->GetName());
			}
			return Loaded;
		}
		UE_LOG(LogCharacterSettings, Warning,
					  TEXT("DefaultRolePawnClass '%s' 를 로드하지 못했습니다."),
					  *DefaultRolePawnClass.ToString());
	}
	UE_LOG(LogCharacterSettings, Warning,
			  TEXT("역할 폰 매핑도 DefaultRolePawnClass 도 비어 있습니다. "
					   "Project Settings → Game → Character 를 채우세요."));

	return nullptr;
}
