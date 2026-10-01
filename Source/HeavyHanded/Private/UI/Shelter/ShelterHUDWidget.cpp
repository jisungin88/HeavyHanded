#include "UI/Shelter/ShelterHUDWidget.h"

#include "Core/HeistSettings.h"
#include "Core/PlayerStates/ShelterPlayerState.h"

void UShelterHUDWidget::TryBind()
{
	if (BoundPlayerState.IsValid())
	{
		return;
	}

	AShelterPlayerState* PS = GetMyShelterPlayerState();
	if (!PS)
	{
		ScheduleRebind();
		return;
	}

	BoundPlayerState = PS;
	PS->OnArrestedChanged.AddDynamic(this, &UShelterHUDWidget::HandleMyArrestedChanged);

	HandleMyArrestedChanged(PS);
}

void UShelterHUDWidget::Unbind()
{
	if (AShelterPlayerState* PS = BoundPlayerState.Get())
	{
		PS->OnArrestedChanged.RemoveDynamic(this, &UShelterHUDWidget::HandleMyArrestedChanged);
	}

	BoundPlayerState = nullptr;
}

void UShelterHUDWidget::HandleMyArrestedChanged(AShelterPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	if (PlayerState->IsArrested())
	{
		if (!bArrestNoticeShown)
		{
			bArrestNoticeShown = true;
			BP_ShowArrestNotice(UHeistSettings::Get()->RescueCost);
		}
	}
	else if (bArrestNoticeShown)
	{
		bArrestNoticeShown = false;
		BP_ShowRescuedNotice();
	}
}
