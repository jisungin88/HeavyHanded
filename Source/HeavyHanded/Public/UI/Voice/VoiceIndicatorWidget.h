#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Voice/VoiceChatComponent.h"
#include "VoiceIndicatorWidget.generated.h"

class UInputAction;

UCLASS(Abstract)
class HEAVYHANDED_API UVoiceIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 마이크 푸시/상시 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Voice")
	void OnModeUpdated(bool bOpenMic);

	/** 마이크 상태 변경 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Voice")
	void OnMicStateUpdated(EHHVoiceMicState NewState);

	/** 마이크 모드 변경 토스트 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Voice")
	void OnModeToggled(bool bOpenMic);

	/** 마이크 활성화 키 */
	UFUNCTION(BlueprintPure, Category = "UI|Voice")
	FText GetPushToTalkKeyText() const;

	/** 마이크 상시/푸시 전환 키 */
	UFUNCTION(BlueprintPure, Category = "UI|Voice")
	FText GetToggleOpenMicKeyText() const;

private:
	UFUNCTION()
	void HandleModeChanged(bool bOpenMic);

	UFUNCTION()
	void HandleMicStateChanged(EHHVoiceMicState NewState);

	FText GetKeyText(const UInputAction* Action) const;

	UPROPERTY()
	TObjectPtr<UVoiceChatComponent> Bound;
};
