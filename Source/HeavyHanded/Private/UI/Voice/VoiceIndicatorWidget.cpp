#include "UI/Voice/VoiceIndicatorWidget.h"

#include "Core/PlayerControllers/HeavyHandedPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Voice/VoiceChatSettings.h"

void UVoiceIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const AHeavyHandedPlayerController* PC = Cast<AHeavyHandedPlayerController>(GetOwningPlayer());
	Bound = PC ? PC->GetVoiceChatComponent() : nullptr;
	if (!Bound)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Bound->OnVoiceModeChanged.AddDynamic(this, &UVoiceIndicatorWidget::HandleModeChanged);
	Bound->OnMicStateChanged.AddDynamic(this, &UVoiceIndicatorWidget::HandleMicStateChanged);

	OnModeUpdated(Bound->IsOpenMic());
	OnMicStateUpdated(Bound->GetMicState());
}

void UVoiceIndicatorWidget::NativeDestruct()
{
	if (Bound)
	{
		Bound->OnVoiceModeChanged.RemoveDynamic(this, &UVoiceIndicatorWidget::HandleModeChanged);
		Bound->OnMicStateChanged.RemoveDynamic(this, &UVoiceIndicatorWidget::HandleMicStateChanged);
		Bound = nullptr;
	}

	Super::NativeDestruct();
}

void UVoiceIndicatorWidget::HandleModeChanged(bool bOpenMic)
{
	OnModeUpdated(bOpenMic);
	OnModeToggled(bOpenMic);
}

void UVoiceIndicatorWidget::HandleMicStateChanged(EHHVoiceMicState NewState)
{
	OnMicStateUpdated(NewState);
}

FText UVoiceIndicatorWidget::GetPushToTalkKeyText() const
{
	return GetKeyText(UVoiceChatSettings::Get()->PushToTalkAction.LoadSynchronous());
}

FText UVoiceIndicatorWidget::GetToggleOpenMicKeyText() const
{
	return GetKeyText(UVoiceChatSettings::Get()->ToggleOpenMicAction.LoadSynchronous());
}

FText UVoiceIndicatorWidget::GetKeyText(const UInputAction* Action) const
{
	const APlayerController* PC = GetOwningPlayer();
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Sub =
		LP ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP) : nullptr;
	if (!Sub || !Action)
	{
		return FText::GetEmpty();
	}

	const TArray<FKey> Keys = Sub->QueryKeysMappedToAction(Action);
	return Keys.IsEmpty() ? FText::GetEmpty() : Keys[0].GetDisplayName();
}
