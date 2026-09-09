#include "Core/GameModes/ShelterGameMode.h"

#include "Core/RunProgressSubsystem.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Core/PlayerControllers/ShelterPlayerController.h"


void AShelterGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

	AShelterPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AShelterPlayerState>() : nullptr;
	const URunProgressSubsystem* Run = URunProgressSubsystem::Get(this);
	if (!PS || !Run)
	{
		return;
	}

	const FString SavedNick = Run->GetNickname(PS->GetUniqueId());
	if (!SavedNick.IsEmpty())
	{
		AShelterGameState* GS = GetGameState<AShelterGameState>();

		if (GS && !GS->IsNicknameTaken(SavedNick, PS))
		{
			ChangeName(NewPlayer, SavedNick, false);
		}
	}

	const FGameplayTag RoleTag = Run->GetSelectedRole(PS->GetUniqueId());
	if (RoleTag.IsValid())
	{
		const EJobType Restored = JobTypeFromRoleTag(RoleTag);
		if (Restored != EJobType::None && PS->GetSelectedJob() == EJobType::None)
		{
			PS->SetSelectedJob(Restored);
		}

		// 이미 확정한 역할이 남아 있는 재입장이다. 선택 화면을 건너뛰어야 하므로
		// 확정 플래그도 같이 복구한다 — 이게 없으면 은신처 페이지로 넘어가지 못한다.
		//
		// RoleTag 는 유효한데 Role.* 4종에 매칭되지 않으면 SelectedJob 이 None 으로 남는다.
		// 그 상태로 확정을 세우면 직업 없이 은신처 페이지로 넘어가므로 실제 직업을 보고 판단한다
		PS->SetJobConfirmed(PS->GetSelectedJob() != EJobType::None);

		if (!NewPlayer->GetPawn())
		{
			if (AShelterPlayerController* SPC = Cast<AShelterPlayerController>(NewPlayer))
			{
				SPC->SpawnJobPawn(RoleTag);
			}
		}
	}
	else
	{
		// 처음 방문 — 역할 기록이 없으니 클라에 직업 선택 팝업을 띄우라고 지시한다.
		// 클라 BP 가 스스로 PlayerState 를 보고 판단하면 복제 타이밍에 걸려 안 뜨는 경우가 있다
		if (AShelterPlayerController* SPC = Cast<AShelterPlayerController>(NewPlayer))
		{
			SPC->ClientShowJobSelect();
		}
	}

    if (AShelterGameState* GS = GetGameState<AShelterGameState>())
    {
        GS->UpdateLobbyPlayerCount();
    }

    //GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
      //  FString::Printf(TEXT("PlayerCount = %d"), GetGameState<AShelterGameState>()->GetLobbyPlayerCount()));

}

void AShelterGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    if (AShelterGameState* GS = GetGameState<AShelterGameState>())
    {
        GS->UpdateLobbyPlayerCount();
    }
}

bool AShelterGameMode::TryApplyNickname(APlayerController* Player, const FString& Raw, ENicknameError& OutError)
{
	OutError = ENicknameError::None;

	APlayerState* PS = Player ? Player->PlayerState : nullptr;
	AShelterGameState* GS = GetGameState<AShelterGameState>();
	if (!PS || !GS)
	{
		return false;
	}

	const FString Clean = AShelterGameState::SanitizeNickname(Raw);

	OutError = AShelterGameState::ValidateNicknameFormat(Clean);
	if (OutError != ENicknameError::None)
	{
		return false;
	}

	if (GS->IsNicknameTaken(Clean, PS))
	{
		OutError = ENicknameError::Taken;
		return false;
	}

	Super::ChangeName(Player, Clean, true);

	if (URunProgressSubsystem* Run = URunProgressSubsystem::Get(this))
	{
		Run->SetNickname(PS->GetUniqueId(), Clean);
	}

	return true;
}

void AShelterGameMode::ChangeName(AController* Controller, const FString& NewName, bool bNameChange)
{
	if (!Controller || !Controller->PlayerState)
	{
		return;
	}

	if (!bNameChange)
	{
		Super::ChangeName(Controller, NewName, bNameChange);
		return;
	}

	ENicknameError Error = ENicknameError::None;
	TryApplyNickname(Cast<APlayerController>(Controller), NewName, Error);
}
