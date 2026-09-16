#include "UI/Shelter/ShelterWidgetBase.h"

#include "Core/GameStates/ShelterGameState.h"
#include "Core/PlayerControllers/ShelterPlayerController.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UShelterWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UShelterWidgetBase::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
	}

	Unbind();
	Super::NativeDestruct();
}

void UShelterWidgetBase::ScheduleRebind()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(BindRetryHandle, this,
		&UShelterWidgetBase::TryBind, BindRetryInterval, false);
}

AShelterGameState* UShelterWidgetBase::GetShelterGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AShelterGameState>() : nullptr;
}

AShelterPlayerState* UShelterWidgetBase::GetMyShelterPlayerState() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPlayerState<AShelterPlayerState>() : nullptr;
}

AShelterPlayerController* UShelterWidgetBase::GetShelterPC() const
{
	return Cast<AShelterPlayerController>(GetOwningPlayer());
}
