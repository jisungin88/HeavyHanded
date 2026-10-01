#include "Voice/VoiceChatComponent.h"

#include "Voice/HeavyGameUserSettings.h"
#include "Voice/VoiceChatSettings.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/VoiceConfig.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystemUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogHHVoice, Log, All);

UVoiceChatComponent::UVoiceChatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVoiceChatComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PC = GetOwningPC();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	const UVoiceChatSettings* Settings = UVoiceChatSettings::Get();
	Attenuation = Settings->VoiceAttenuation.LoadSynchronous();
	UE_CLOG(!Attenuation, LogHHVoice, Warning,
	        TEXT("VoiceAttenuation 이 비어 있어 음성이 거리와 상관없이 들립니다. Project Settings > Game > Voice Chat에서 설정"));
	UE_CLOG(!UHeavyGameUserSettings::Get(), LogHHVoice, Warning,
	        TEXT("GameUserSettingsClassName 이 HeavyGameUserSettings 가 아닙니다. 상시 마이크 옵션이 저장되지 않습니다."));

	if (IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld()))
	{
		TalkingHandle = Voice->AddOnPlayerTalkingStateChangedDelegate_Handle(
			FOnPlayerTalkingStateChangedDelegate::CreateUObject(this, &UVoiceChatComponent::HandleTalkingStateChanged));
	}

	GetWorld()->GetTimerManager().SetTimer(TickHandle, this,
		&UVoiceChatComponent::TickVoice,
		Settings->TalkerRefreshInterval, true, 0.f);
}

void UVoiceChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TalkingHandle.IsValid())
	{
		if (IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld()))
		{
			Voice->ClearOnPlayerTalkingStateChangedDelegate_Handle(TalkingHandle);
		}
		TalkingHandle.Reset();
	}
	bSpeaking = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickHandle);
	}

	bPushToTalkHeld = false;
	if (bTransmitting)
	{
		if (APlayerController* PC = GetOwningPC())
		{
			PC->ToggleSpeaking(false);
		}
		bTransmitting = false;
	}
	Talkers.Reset();
	Super::EndPlay(EndPlayReason);
}

void UVoiceChatComponent::OnPushToTalkPressed()
{
	bPushToTalkHeld = true;
	UpdateTransmitting();
}

void UVoiceChatComponent::OnPushToTalkReleased()
{
	bPushToTalkHeld = false;
	UpdateTransmitting();
}

bool UVoiceChatComponent::IsOpenMic() const
{
	const UHeavyGameUserSettings* UserSettings = UHeavyGameUserSettings::Get();
	return UserSettings && UserSettings->IsVoiceOpenMic();
}

void UVoiceChatComponent::SetOpenMic(bool bEnable)
{
	UHeavyGameUserSettings* UserSettings = UHeavyGameUserSettings::Get();
	if (!UserSettings || UserSettings->IsVoiceOpenMic() == bEnable)
	{
		return;
	}

	UserSettings->SetVoiceOpenMic(bEnable);
	UserSettings->SaveSettings();
	OnVoiceModeChanged.Broadcast(bEnable);

	UpdateTransmitting();
}

void UVoiceChatComponent::ToggleOpenMic()
{
	SetOpenMic(!IsOpenMic());
}

APlayerController* UVoiceChatComponent::GetOwningPC() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UVoiceChatComponent::CanTransmit() const
{
	const APlayerController* PC = GetOwningPC();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	return PS && !PS->IsOnlyASpectator();
}

void UVoiceChatComponent::UpdateTransmitting()
{
	APlayerController* PC = GetOwningPC();

	if (!PC || !PC->GetLocalPlayer())
	{
		return;
	}

	const bool bWant  = (IsOpenMic() || bPushToTalkHeld) && CanTransmit();
	if (bWant == bTransmitting)
	{
		return;
	}

	PC->ToggleSpeaking(bWant);
	bTransmitting = bWant;

	if (!bWant)
	{
		bSpeaking = false;
	}
	RefreshMicState();
}

void UVoiceChatComponent::TickVoice()
{
	RefreshTalkers();
	UpdateTransmitting();
	RefreshMicState();
}

void UVoiceChatComponent::RefreshTalkers()
{
	const APlayerController* PC = GetOwningPC();
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!PC || !GS)
	{
		return;
	}

	for (auto It = Talkers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (APlayerState* Other : GS->PlayerArray)
	{
		if (!IsValid(Other) || Other == PC->PlayerState || !Other->GetUniqueId().IsValid())
		{
			continue;
		}

		TObjectPtr<UVOIPTalker>& Talker = Talkers.FindOrAdd(Other);
		if (!Talker)
		{
			Talker = UVOIPTalker::CreateTalkerForPlayer(Other);
		}

		const APawn* Pawn = Other->GetPawn();
		Talker->Settings.ComponentToAttachTo = Pawn ? Pawn->GetRootComponent() : nullptr;
		Talker->Settings.AttenuationSettings = Pawn ? Attenuation.Get() : nullptr;
	}
}



void UVoiceChatComponent::HandleTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking)
{
	const APlayerController* PC = GetOwningPC();
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	const FUniqueNetIdRepl LocalId = LP ? LP->GetPreferredUniqueNetId() : FUniqueNetIdRepl();
	if (!LocalId.IsValid() || !(LocalId == *TalkerId))
	{
		return;
	}

	bSpeaking = bIsTalking && bTransmitting;
	RefreshMicState();
}

void UVoiceChatComponent::RefreshMicState()
{
	const APlayerController* PC = GetOwningPC();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;

	EHHVoiceMicState NewState = EHHVoiceMicState::Off;
	if (PS && PS->IsOnlyASpectator())
	{
		NewState = EHHVoiceMicState::Muted;
	}
	else if (bTransmitting)
	{
		NewState = bSpeaking ? EHHVoiceMicState::Speaking : EHHVoiceMicState::Open;
	}

	if (NewState == MicState)
	{
		return;
	}

	MicState = NewState;
	OnMicStateChanged.Broadcast(MicState);
}
