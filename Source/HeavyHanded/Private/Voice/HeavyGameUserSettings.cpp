#include "Voice/HeavyGameUserSettings.h"

#include "Engine/Engine.h"

UHeavyGameUserSettings* UHeavyGameUserSettings::Get()
{
	return GEngine ? Cast<UHeavyGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}
