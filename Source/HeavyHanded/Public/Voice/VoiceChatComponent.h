#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Online/CoreOnlineFwd.h"
#include "VoiceChatComponent.generated.h"

class APlayerController;
class APlayerState;
class USoundAttenuation;
class UVOIPTalker;

/** 마이크 표시 상태 */
UENUM(BlueprintType)
enum class EHHVoiceMicState : uint8
{
	Off,
	Open,
	Speaking,
	Muted,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceModeChanged, bool, bOpenMic);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceMicStateChanged, EHHVoiceMicState, NewState);

UCLASS(ClassGroup=(Voice), meta=(BlueprintSpawnableComponent))
class HEAVYHANDED_API UVoiceChatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVoiceChatComponent();

	void OnPushToTalkPressed();
	void OnPushToTalkReleased();

	UFUNCTION(BlueprintPure, Category = "HeavyHanded|Voice")
	bool IsOpenMic() const;

	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Voice")
	void SetOpenMic(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "HeavyHanded|Voice")
	bool IsTransmitting() const { return bTransmitting; }

	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Voice")
	void ToggleOpenMic();

	UFUNCTION(BlueprintPure, Category = "HeavyHanded|Voice")
	EHHVoiceMicState GetMicState() const { return MicState; }

	UPROPERTY(BlueprintAssignable, Category = "HeavyHanded|Voice")
	FOnVoiceModeChanged OnVoiceModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "HeavyHanded|Voice")
	FOnVoiceMicStateChanged OnMicStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetOwningPC() const;

	/** 송출 가능 여부 */
	bool CanTransmit() const;

	/** 송출 상태와 실제 상태를 맞춘다 */
	void UpdateTransmitting();

	void RefreshTalkers();
	void TickVoice();

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<APlayerState>, TObjectPtr<UVOIPTalker>> Talkers;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;

	FTimerHandle TickHandle;
	bool bPushToTalkHeld = false;
	bool bTransmitting = false;

	void HandleTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking);

	void RefreshMicState();

	FDelegateHandle TalkingHandle;
	bool bSpeaking = false;
	EHHVoiceMicState MicState = EHHVoiceMicState::Off;

};
