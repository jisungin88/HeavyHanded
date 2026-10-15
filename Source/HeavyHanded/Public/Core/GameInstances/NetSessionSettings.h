#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "NetSessionSettings.generated.h"

class UWorld;

UCLASS(config = NetSession, defaultconfig, meta = (DisplayName = "Session"))
class HEAVYHANDED_API UNetSessionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UNetSessionSettings* Get() { return GetDefault<UNetSessionSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config,  EditAnywhere, Category = "Room", meta = (ClampMin = "1"))
	int32 MinPlayers = 2;

	UPROPERTY(Config, EditAnywhere, Category = "Room", meta = (ClampMin = "1"))
	int32 MaxPlayers = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Room")
	FString DefaultRoomNamePrefix = TEXT("New World");

	UPROPERTY(Config, EditAnywhere, Category = "Room", meta = (ClampMin = "1", ClampMax = "32"))
	int32 RandomNameSuffixLength = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Room", meta = (ClampMin = "1", ClampMax = "12"))
	int32 RoomCodeLength = 6;

	UPROPERTY(Config, EditAnywhere, Category = "Room")
	FString RoomCodeChars = TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");

	UPROPERTY(Config, EditAnywhere, Category = "Room", meta = (ClampMin = "1"))
	int32 MaxSearchResults = 20;

	UPROPERTY(Config, EditAnywhere, Category = "Travel")
	TSoftObjectPtr<UWorld> TitleMap;

	UPROPERTY(Config, EditAnywhere, Category = "Travel")
	TSoftObjectPtr<UWorld> ShelterMap;
};
