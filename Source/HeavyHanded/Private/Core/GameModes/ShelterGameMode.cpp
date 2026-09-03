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

	const FGameplayTag RoleTag = Run->GetSelectedRole(PS->GetUniqueId());
	if (RoleTag.IsValid())
	{
		const EJobType Restored = JobTypeFromRoleTag(RoleTag);
		if (Restored != EJobType::None && PS->GetSelectedJob() == EJobType::None)
		{
			PS->SetSelectedJob(Restored);
		}

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
		// 처음 방문
		if (AShelterPlayerController* SPC = Cast<AShelterPlayerController>(NewPlayer))
		{
			SPC->ClientShowJobSelect();
		}
	}

	const EJobType Restored = JobTypeFromRoleTag(RoleTag);
	if (Restored != EJobType::None)
	{
		if (PS->GetSelectedJob() == EJobType::None)
		{
			PS->SetSelectedJob(Restored);
		}

		if (!NewPlayer->GetPawn())
		{
			if (AShelterPlayerController* SPC = Cast<AShelterPlayerController>(NewPlayer))
			{
				SPC->SpawnJobPawn(RoleTag);
			}
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
