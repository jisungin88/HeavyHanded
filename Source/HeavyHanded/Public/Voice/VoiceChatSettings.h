#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoiceChatSettings.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundAttenuation;

UCLASS(config = VoiceChat, defaultconfig, meta = (DisplayName = "Voice Chat"))
class HEAVYHANDED_API UVoiceChatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UVoiceChatSettings* Get() { return GetDefault<UVoiceChatSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 근접 음성 감쇠 */
	UPROPERTY(config, EditAnywhere, Category = "Proximity")
	TSoftObjectPtr<USoundAttenuation> VoiceAttenuation;

	/** 다른 플레이어 음성을 폰에 붙이는 주기 */
	UPROPERTY(config, EditAnywhere, Category = "Proximity", meta = (ClampMin = "0.1", Units = "s"))
	float TalkerRefreshInterval = 0.5f;

	UPROPERTY(config, EditAnywhere, Category = "Input", meta = (AllowedClasses = "/Script/EnhancedInput.InputMappingContext"))
	TSoftObjectPtr<UInputMappingContext> VoiceMappingContext;

	UPROPERTY(config, EditAnywhere, Category = "Input", meta = (AllowedClasses = "/Script/EnhancedInput.InputAction"))
	TSoftObjectPtr<UInputAction> PushToTalkAction;

	UPROPERTY(config, EditAnywhere, Category = "Input", meta = (AllowedClasses = "/Script/EnhancedInput.InputAction"))
	TSoftObjectPtr<UInputAction> ToggleOpenMicAction;

	UPROPERTY(config, EditAnywhere, Category = "Input")
	int32 VoiceInputPriority = 50;
};
