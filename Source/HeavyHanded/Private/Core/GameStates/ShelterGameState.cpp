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

	// 직업 변경 이벤트를 받을 준비
	AShelterPlayerState* ShelterPlayerState = Cast<AShelterPlayerState>(PlayerState);
	if (ShelterPlayerState)
	{
		ShelterPlayerState->OnSelectedJobChanged.AddDynamic(this, &AShelterGameState::OnPlayerJobChanged);
	}

	UpdateLobbyPlayerCount();
	UpdateCanStart();
}

void AShelterGameState::RemovePlayerState(APlayerState* PlayerState)
{
	AShelterPlayerState* ShelterPlayerState = Cast<AShelterPlayerState>(PlayerState);

	if (ShelterPlayerState)
	{
		ShelterPlayerState->OnSelectedJobChanged.RemoveDynamic(this, &AShelterGameState::OnPlayerJobChanged);
	}


	Super::RemovePlayerState(PlayerState);

	UpdateLobbyPlayerCount();
	UpdateCanStart();
}

void AShelterGameState::UpdateLobbyPlayerCount()
{
	const int32 PlayerCount = PlayerArray.Num();

	OnLobbyPlayerCountChanged.Broadcast(PlayerCount);
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

	if (GEngine)
	{
		FString Message = FString::Printf(
			TEXT("[2 GS] PS=%p / %s / Authority=%s"),
			PlayerState,
			*PlayerState->GetName(),
			HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT")
		);

		GEngine->AddOnScreenDebugMessage(
			-1, 10.f, FColor::Yellow, Message
		);
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




	UE_LOG(LogTemp, Display,
		TEXT("[SET JOB] PS=%p Name=%s Authority=%s Job=%s"),
		this,
		*GetName(),
		HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		*UEnum::GetValueAsString(NewJob));




	// 직업 변경
	PlayerState->SetSelectedJob(NewJob);









	// 값 자체를 변경해야 클라이언트에서 OnRep가 실행됨
	JobStateChanged++;
	//OnJobStateChanged.Broadcast();





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

bool AShelterGameState::CanStartGame() const
{
	for (APlayerState* PS : PlayerArray)
	{
		AShelterPlayerState* ShelterPS = Cast<AShelterPlayerState>(PS);

		if (!ShelterPS)
		{
			continue;
		}

		if (ShelterPS->GetSelectedJob() == EJobType::None)
		{
			return false;
		}
	}

	return PlayerArray.Num() > 0;
}

void AShelterGameState::UpdateCanStart()
{
	if (!HasAuthority())
	{
		return;
	}

	bCanStart = CanStartGame();
}



void AShelterGameState::OnRep_JobStateChanged()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Client GameState JobState Changed"));
	}

	OnJobStateChanged.Broadcast();
}



TArray<AShelterPlayerState*> AShelterGameState::GetShelterPlayerStates() const
{
	TArray<AShelterPlayerState*> Result;

	for (APlayerState* BasePlayerState : PlayerArray)
	{
		AShelterPlayerState* PlayerState = Cast<AShelterPlayerState>(BasePlayerState);

		if (PlayerState)
		{
			Result.Add(PlayerState);
		}
	}

	return Result;
}

//
void AShelterGameState::OnPlayerJobChanged(AShelterPlayerState* PlayerState)
{
	OnJobStateChanged.Broadcast();
}



// ---------------
// GameState 자체는 클라이언트 소유 액터가 아니므로
// 여기서 직접 Server RPC를 호출하는 구조는 적합하지 않음
// ----------------

void AShelterGameState::SetEntryTag(EEntryTag NewTag)
{
	// GameState의 공용 값은 서버에서만 변경
	if (!HasAuthority())
	{
		return;
	}

	// 현재 Entry 변경
	EntryTag = NewTag;

	// 호스트의 UI 갱신
	OnTravelTagChanged.Broadcast();
}

void AShelterGameState::SetSiteTag(ESiteTag NewTag)
{
	// GameState의 공용 값은 서버에서만 변경
	if (!HasAuthority())
	{
		return;
	}

	// 현재 Site 변경
	SiteTag = NewTag;

	// 호스트의 UI는 RepNotify가 자동으로 호출되지 않으므로 직접 알림
	OnTravelTagChanged.Broadcast();
}



void AShelterGameState::OnRep_EntryTag()
{
	// 클라이언트에서 EntryTag가 복제되면 UI 갱신
	OnTravelTagChanged.Broadcast();

	UE_LOG(LogTemp, Warning, TEXT(
		"[SiteTag] OnRep 실행 | World=%s | NetMode=%d | Tag=%d"
	), *GetWorld()->GetName(), (int32)GetNetMode(), (int32)SiteTag);
}

void AShelterGameState::OnRep_SiteTag()
{
	// 클라이언트에서 SiteTag가 복제되면 UI 갱신
	OnTravelTagChanged.Broadcast();
}



void AShelterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShelterGameState, JobStateChanged);

	DOREPLIFETIME(AShelterGameState, SiteTag);
	DOREPLIFETIME(AShelterGameState, EntryTag);
}
