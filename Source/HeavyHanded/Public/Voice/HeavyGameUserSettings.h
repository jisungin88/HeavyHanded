#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "HeavyGameUserSettings.generated.h"

UCLASS()
class HEAVYHANDED_API UHeavyGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static UHeavyGameUserSettings* Get();

	UFUNCTION(BlueprintPure, Category = "HeavyHanded|Voice")
	bool IsVoiceOpenMic() const { return bVoiceOpenMic; }

	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Voice")
	void SetVoiceOpenMic(bool bEnable) { bVoiceOpenMic = bEnable; }

protected:
	/** true는 상시, false는 푸쉬 */
	UPROPERTY(config)
	bool bVoiceOpenMic = false;
};
