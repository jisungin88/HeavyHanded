#include "UI/Shelter/JobSelectWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Core/PlayerControllers/ShelterPlayerController.h"

#include "UI/HeavyUIText.h"

void UJobSelectWidget::TryBind()
{
	if (BoundState && BoundPlayerState)
	{
		return;
	}

	AShelterGameState* GS = GetShelterGameState();
	AShelterPlayerState* PS = GetMyShelterPlayerState();

	if (!GS || !PS)
	{
		ScheduleRebind();
		return;
	}

	BoundState = GS;
	BoundPlayerState = PS;

	GS->OnJobStateChanged.AddDynamic(this, &UJobSelectWidget::HandleJobStateChanged);
	PS->OnSelectedJobChanged.AddDynamic(this, &UJobSelectWidget::HandleSelectedJobChanged);

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.AddDynamic(this, &UJobSelectWidget::HandleConfirmClicked);
	}

	if (Input_Nickname)
	{
		Input_Nickname->OnTextChanged.AddDynamic(this, &UJobSelectWidget::HandleNicknameChanged);
	}

	PS->OnNicknameRejected.AddDynamic(this, &UJobSelectWidget::HandleNicknameRejected);

	Refresh();
}

void UJobSelectWidget::Unbind()
{
	if (AShelterGameState* GS = BoundState.Get())
	{
		GS->OnJobStateChanged.RemoveDynamic(this, &UJobSelectWidget::HandleJobStateChanged);
	}

	if (AShelterPlayerState* PS = BoundPlayerState.Get())
	{
		PS->OnSelectedJobChanged.RemoveDynamic(this, &UJobSelectWidget::HandleSelectedJobChanged);
		PS->OnNicknameRejected.RemoveDynamic(this, &UJobSelectWidget::HandleNicknameRejected);
	}

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.RemoveDynamic(this, &UJobSelectWidget::HandleConfirmClicked);
	}

	if (Input_Nickname)
	{
		Input_Nickname->OnTextChanged.RemoveDynamic(this, &UJobSelectWidget::HandleNicknameChanged);
	}

	BoundState = nullptr;
	BoundPlayerState = nullptr;
}

void UJobSelectWidget::RequestSelectJob(EJobType NewJob)
{
	if (AShelterPlayerController* PC = GetShelterPC())
	{
		PC->ServerSelectJob(NewJob);
	}
}

void UJobSelectWidget::RequestConfirmJob()
{
	if (!CanConfirm())
	{
		return;
	}

	FString Clean;
	if (CheckNickname(Clean) != ENicknameError::None)
	{
		return;
	}

	if (AShelterPlayerController* PC = GetShelterPC())
	{
		PC->serverConfirmedJob(Clean);
	}
}

bool UJobSelectWidget::IsJobTaken(EJobType Job) const
{
	const AShelterGameState* GS = BoundState.Get();
	return GS ? GS->IsJobAlreadySelected(Job) : false;
}

EJobType UJobSelectWidget::GetMySelectedJob() const
{
	const AShelterPlayerState* PS = BoundPlayerState.Get();
	return PS ? PS->GetSelectedJob() : EJobType::None;
}

bool UJobSelectWidget::CanConfirm() const
{
	const AShelterPlayerState* PS = BoundPlayerState.Get();
	if (!PS || PS->GetSelectedJob() == EJobType::None || PS->IsJobConfirmed())
	{
		return false;
	}

	FString Clean;
	return CheckNickname(Clean) == ENicknameError::None;
}

void UJobSelectWidget::HandleJobStateChanged()
{
	Refresh();
}

void UJobSelectWidget::HandleSelectedJobChanged(AShelterPlayerState* PlayerState)
{
	Refresh();
}

void UJobSelectWidget::HandleConfirmClicked()
{
	RequestConfirmJob();
}

void UJobSelectWidget::HandleNicknameChanged(const FText& Text)
{
	FString Clean;
	const ENicknameError Error = CheckNickname(Clean);

	OnNicknameFeedback(HeavyUIText::NicknameError(Error), Error == ENicknameError::None);

	Refresh();
}

void UJobSelectWidget::HandleNicknameRejected(ENicknameError Error)
{
	OnNicknameFeedback(HeavyUIText::NicknameError(Error), false);
}

ENicknameError UJobSelectWidget::CheckNickname(FString& OutClean) const
{
	const FString Raw = Input_Nickname ? Input_Nickname->GetText().ToString() : FString();
	OutClean = AShelterGameState::SanitizeNickname(Raw);

	const ENicknameError FormatError = AShelterGameState::ValidateNicknameFormat(OutClean);
	if (FormatError != ENicknameError::None)
	{
		return FormatError;
	}

	const AShelterGameState* GS = GetShelterGameState();
	if (GS && GS->IsNicknameTaken(OutClean, GetMyShelterPlayerState()))
	{
		return ENicknameError::Taken;
	}

	return ENicknameError::None;
}

void UJobSelectWidget::Refresh()
{
	if (Btn_Confirm)
	{
		Btn_Confirm->SetIsEnabled(CanConfirm());
	}

	OnJobStateRefreshed();
}
