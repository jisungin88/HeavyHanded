// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameStates/ShelterGameState.h"
#include "Net/UnrealNetwork.h"

int32 AShelterGameState::GetLobbyPlayerCount() const
{
	return PlayerArray.Num();
}

void AShelterGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	UpdateLobbyPlayerCount();
}

void AShelterGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	UpdateLobbyPlayerCount();
}

void AShelterGameState::UpdateLobbyPlayerCount()
{
	const int32 PlayerCount = PlayerArray.Num();

	OnLobbyPlayerCountChanged.Broadcast(PlayerCount);
}

void AShelterGameState::OnRep_JobStateChanged()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Client GameState JobState Changed"));
	}

	OnJobStateChanged.Broadcast();
}

void AShelterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShelterGameState, JobStateChanged);
}

// 해당 직업을 누군가 이미 선택했는지 검사
bool AShelterGameState::IsJobAlreadySelected(EJobType Job) const
{
	// None은 실제 직업이 아니기 때문에 검사할 필요 없음
	if (Job == EJobType::None)
	{
		return false;
	}


	// GameState의 PlayerArray에는 현재 접속한
	// 모든 플레이어의 PlayerState가 들어 있음
	for (APlayerState* BasePlayerState : PlayerArray)
	{
		// 우리 PlayerState로 형변환
		AShelterPlayerState* PlayerState = Cast<AShelterPlayerState>(BasePlayerState);

		// 변환 실패하면 다음 플레이어 검사
		if (!PlayerState)
		{
			continue;
		}


		// 해당 플레이어가 현재 이 직업을 가지고 있는지 확인
		if (PlayerState->GetSelectedJob() == Job)
		{
			// 이미 선택한 사람이 있음
			return true;
		}
	}


	// 아무도 선택하지 않았다면 선택 가능
	return false;
}

// 해당 직업을 선택할 수 있는지 검사
bool AShelterGameState::CanSelectJob(EJobType Job) const
{
	// None은 선택 불가능
	if (Job == EJobType::None)
	{
		return false;
	}


	// 이미 다른 플레이어가 선택한 직업이면 불가능
	if (IsJobAlreadySelected(Job))
	{
		return false;
	}


	// 선택 가능
	return true;
}

// 직업 실제 선택
bool AShelterGameState::SelectJob(AShelterPlayerState* PlayerState, EJobType NewJob)
{
	// 반드시 서버에서만 실행
	if (!HasAuthority())
	{
		return false;
	}




	// PlayerState가 없으면 실패
	if (!PlayerState)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("GS -> PlayerState 없음"));
		}
		return false;
	}


	// None은 선택 불가능
	if (NewJob == EJobType::None)
	{
		return false;
	}


	// 현재 자기 직업과 같은 직업을 다시 누른 경우
	// 이 경우 실패시킬 필요가 없으므로 성공 처리 // 그냥 비활성화할 것
	// --------------------------------------------------------
	if (PlayerState->GetSelectedJob() == NewJob)
	{
		return true;
	}


	// 다른 플레이어가 이미 이 직업을 선택했는지 검사
	if (IsJobAlreadySelected(NewJob))
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("GS -> 이미 선택된 직업입니다."));
		return false;
	}

	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("2. GS -> SelectJob 호출"));

	// 직업 변경
	PlayerState->SetSelectedJob(NewJob);

	// 값 자체를 변경해야 클라이언트에서 OnRep가 실행됨
	JobStateChanged++;
	OnJobStateChanged.Broadcast();

	return true;

}

bool AShelterGameState::ClearJob(AShelterPlayerState* PlayerState)
{
	// 서버에서만 실행
	if (!HasAuthority())
	{
		return false;
	}


	if (!PlayerState)
	{
		return false;
	}


	// 직업을 None으로 변경
	PlayerState->SetSelectedJob(EJobType::None);

	return true;
}

