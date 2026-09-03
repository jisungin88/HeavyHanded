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
