// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/GameStates/ShelterGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameplayTagsManager.h"
#include "Core/RunProgressSubsystem.h"
#include "Core/HeistLog.h"
#include "Core/HeistSettings.h"
#include "UI/UISettings.h"         // GetSiteDisplayName — 장소 이름의 폴백 문구가 거기 있다
#include "HAL/IConsoleManager.h"
#include "Shared/NetAuthority.h"

int32 AShelterGameState::GetLobbyPlayerCount() const
{
	return PlayerArray.Num();
}

void AShelterGameState::PublishRunProgress()
{
	if (!HasAuthority())
	{
		return;
	}

	const URunProgressSubsystem* Run = URunProgressSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogHeist, Warning,
		       TEXT("URunProgressSubsystem 이 없어 캠페인 진행을 게시하지 못했습니다."));
		return;
	}

	RunProgress = Run->MakeProgressView();

	EnsureEntrySelected();

	OnRep_RunProgress();

	UE_LOG(LogHeist, Log,
	       TEXT("캠페인 진행 게시 — %d/%d 통과 / 다음 %s / 팀 골드 $%d / 구출 대기 %d명"),
	       RunProgress.ProgressNum, RunProgress.SiteNum,
	       RunProgress.NextSite.IsValid() ? *RunProgress.NextSite.ToString() : TEXT("(없음)"),
	       RunProgress.TeamGold, RunProgress.ArrestedNum);
}

void AShelterGameState::OnRep_RunProgress()
{
	OnRunProgressChanged.Broadcast(RunProgress);
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
	// 작동 확인 후 지울 것
	/*
	AShelterPlayerState* ShelterPlayerState = Cast<AShelterPlayerState>(PlayerState);

	if (ShelterPlayerState)
	{
		ShelterPlayerState->OnSelectedJobChanged.RemoveDynamic(this, &AShelterGameState::OnPlayerJobChanged);
	}


	Super::RemovePlayerState(PlayerState);

	UpdateLobbyPlayerCount();
	UpdateCanStart();

	*/

	AShelterPlayerState* ShelterPlayerState = Cast<AShelterPlayerState>(PlayerState);

	if (ShelterPlayerState)
	{
		// 퇴장하는 플레이어의 직업을 None으로 변경
		ClearJob(ShelterPlayerState);

		// 직업 변경 이벤트 연결 해제
		ShelterPlayerState->OnSelectedJobChanged.RemoveDynamic(
			this,
			&AShelterGameState::OnPlayerJobChanged
		);
	}

	// PlayerArray에서 실제로 제거
	Super::RemovePlayerState(PlayerState);

	UpdateLobbyPlayerCount();
	UpdateCanStart();


	//JobStateChanged++;
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
	UpdateCanStart();


	// 값 자체를 변경해야 클라이언트에서 OnRep가 실행됨
	JobStateChanged++;
	//OnJobStateChanged.Broadcast();
	OnRep_CanStart(); // 서버


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

void AShelterGameState::OnRep_CanStart()
{
	// 클라이언트에서 이벤트 발생
	OnCanStartChanged.Broadcast(bCanStart);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("OnRep_CanStart : %s"),
		bCanStart ? TEXT("TRUE") : TEXT("FALSE")
	);
}

bool AShelterGameState::CanStartGame() const
{
	// 0902 아래 변경 작동확인될시 지울 것
	/*
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
	*/


	/*
	if (PlayerArray.Num() <= 0)
	{
		return false;
	}

	for (APlayerState* PS : PlayerArray)
	{
		AShelterPlayerState* ShelterPS = Cast<AShelterPlayerState>(PS);

		if (!ShelterPS)
		{
			return false;
		}

		if (ShelterPS->GetSelectedJob() == EJobType::None)
		{
			return false;
		}
	}

	return true;


	*/


	// 디버그용
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("========== CanStartGame ==========")
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("PlayerArray Num = %d"),
		PlayerArray.Num()
	);

	if (PlayerArray.Num() <= 0)
	{
		return false;
	}

	for (APlayerState* PS : PlayerArray)
	{
		if (!PS)
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerState is nullptr"));
			return false;
		}

		AShelterPlayerState* ShelterPS = Cast<AShelterPlayerState>(PS);

		if (!ShelterPS)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("PlayerState Cast Failed! Class = %s"),
				*PS->GetClass()->GetName()
			);

			continue;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PS = %s / Job = %s"),
			*ShelterPS->GetName(),
			*UEnum::GetValueAsString(ShelterPS->GetSelectedJob())
		);

		if (!ShelterPS->IsJobConfirmed())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Cannot Start: %s has no Job"),
				*ShelterPS->GetName()
			);

			return false;
		}
	}

	return true;
}

void AShelterGameState::UpdateCanStart()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bNewCanStart = CanStartGame();

	if (bCanStart == bNewCanStart)
	{
		return;
	}

	bCanStart = bNewCanStart;

	// 서버에서도 이벤트 발생
	OnCanStartChanged.Broadcast(bCanStart);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("UpdateCanStart : %s"),
		bCanStart ? TEXT("TRUE") : TEXT("FALSE")
	);
}


void AShelterGameState::OnRep_JobStateChanged()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Client GameState JobState Changed"));
	}

	OnJobStateChanged.Broadcast();
}

void AShelterGameState::NotifyRosterDirty()
{
	if (HasAuthority())
	{
		++JobStateChanged;
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

TArray<FString> AShelterGameState::GetUnconfirmedPlayerNames() const
{
	TArray<FString> Names;
	for (const APlayerState* PS : PlayerArray)
	{
		const AShelterPlayerState* ShelterPS = Cast<AShelterPlayerState>(PS);
		if (ShelterPS && !ShelterPS->IsJobConfirmed())
		{
			Names.Add(ShelterPS->GetName());
		}
	}
	return Names;
}


// ---------------
// GameState 자체는 클라이언트 소유 액터가 아니므로
// 여기서 직접 Server RPC를 호출하는 구조는 적합하지 않음
// ----------------

void AShelterGameState::SetSelectedEntry(FGameplayTag NewEntry)
{
	if (!HasAuthority())
	{
		return;
	}

	if (URunProgressSubsystem* Run = URunProgressSubsystem::Get(this))
	{
		if (NewEntry.IsValid())
		{
			Run->TrySelectEntry(NewEntry);
		}
		else
		{
			Run->ClearSelectedEntry();
		}
	}

	if (SelectedEntry == NewEntry)
	{
		return;
	}

	SelectedEntry = NewEntry;

	OnTravelTagChanged.Broadcast();
}

TArray<FHeistEntryOption> AShelterGameState::GetEntryOptions() const
{
	return UHeistSettings::Get()->GetSiteEntries(RunProgress.NextSite);
}

void AShelterGameState::EnsureEntrySelected()
{
	if (!HasAuthority())
	{
		return;
	}

	FHeistEntryOption Found;
	if (FindEntryOption(SelectedEntry, Found))
	{
		return;
	}

	const TArray<FHeistEntryOption> Options = GetEntryOptions();

	SetSelectedEntry(Options.IsEmpty() ? FGameplayTag() : Options[0].EntryTag);
}

FGameplayTag AShelterGameState::GetNextEntryOption() const
{
	const TArray<FHeistEntryOption> Options = GetEntryOptions();
	if (Options.IsEmpty())
	{
		return FGameplayTag();
	}

	int32 Index = INDEX_NONE;
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		if (Options[i].EntryTag == SelectedEntry)
		{
			Index = i;
			break;
		}
	}

	return Options[(Index + 1) % Options.Num()].EntryTag;
}

bool AShelterGameState::FindEntryOption(FGameplayTag EntryTag, FHeistEntryOption& OutOption) const
{
	if (!EntryTag.IsValid())
	{
		return false;
	}

	for (const FHeistEntryOption& Option : GetEntryOptions())
	{
		if (Option.EntryTag == EntryTag)
		{
			OutOption = Option;
			return true;
		}
	}

	return false;
}

FHeistSiteView AShelterGameState::GetSiteView(FGameplayTag SiteTag) const
{
	FHeistSiteView View;
	View.SiteTag = SiteTag;

	// 이름은 UUISettings 를 거친다 — 표에 행이 없을 때의 폴백 문구("작업 장소")가 거기 있고,
	// 그 문구는 UI 소관이다. 이름 자체의 진리원은 UUISettings 안에서도 DT_SiteCatalog 다
	View.DisplayName = UUISettings::Get()->GetSiteDisplayName(SiteTag);

	const UHeistSettings* Settings = UHeistSettings::Get();
	const FHeistSiteRow* Site = Settings->FindSite(SiteTag);
	if (!Site)
	{
		// 등록되지 않은 장소. bReady 가 거짓이라 출발 문이 잠긴다
		return View;
	}

	View.Description  = Site->Description;
	View.Image        = Site->Image;
	View.TargetValue  = Site->TargetValue;
	View.HeistSeconds = Site->HeistSeconds;
	View.EscapeSeconds= Site->EscapeSeconds;
	View.EntryNum     = Site->Entries.Num();

	// ── 여기부터는 표에 없는 값이다 ──
	View.bCleared = RunProgress.ClearedSites.Contains(SiteTag);

	// 레벨이 없거나 목표가 0 이면 떠나도 판이 성립하지 않는다.
	// 위젯이 이걸 보고 버튼을 잠근다 — 눌렀는데 아무 일도 안 일어나는 것보다 낫다
	View.bReady = !Site->Level.IsNull() && Site->TargetValue > 0;

	return View;
}

FHeistSiteView AShelterGameState::GetNextSiteView() const
{
	// 목표는 캠페인 진행이 정한다. RunProgress 는 복제되므로 클라이언트에서도 유효하다
	return GetSiteView(RunProgress.NextSite);
}

void AShelterGameState::OnRep_SelectedEntry()
{
	// 클라이언트에서 EntryTag가 복제되면 UI 갱신
	OnTravelTagChanged.Broadcast();
}

void AShelterGameState::DebugPrintGameplayTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	FGameplayTagContainer AllTags;
	Manager.RequestAllGameplayTags(AllTags, true);

	for (const FGameplayTag& Tag : AllTags)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameplayTag: %s"), *Tag.ToString());
	}
}


void AShelterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShelterGameState, JobStateChanged);
	DOREPLIFETIME(AShelterGameState, bCanStart);

	DOREPLIFETIME(AShelterGameState, SelectedEntry);

	DOREPLIFETIME(AShelterGameState, RunProgress);
}

FString AShelterGameState::SanitizeNickname(const FString& Raw)
{
	const FString Trimmed = Raw.TrimStartAndEnd();

	FString Result;
	Result.Reserve(Trimmed.Len());
	for (int32 i = 0; i < Trimmed.Len(); ++i)
	{
		if (Trimmed[i] >= 0x20)
		{
			Result.AppendChar(Trimmed[i]);
		}
	}

	return Result;
}

ENicknameError AShelterGameState::ValidateNicknameFormat(const FString& Clean)
{
	if (Clean.IsEmpty())
	{
		return ENicknameError::Empty;
	}
	else if (Clean.Len() > MaxNicknameLength)
	{
		return ENicknameError::TooLong;
	}
	else if (Clean.Len() < MinNicknameLength)
	{
		return ENicknameError::TooShort;
	}
	return ENicknameError::None;
}

bool AShelterGameState::IsNicknameTaken(const FString& Clean, const APlayerState* Exclude) const
{
	if (Clean.IsEmpty())
	{
		return false;
	}

	for (const APlayerState* PS : PlayerArray)
	{
		if (!PS || PS == Exclude)
		{
			continue;
		}

		if (PS->GetPlayerName().Equals(Clean, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

static void RunClearSiteCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!HasServerAuthority(World))
	{
		UE_LOG(LogHeist, Warning, TEXT("이 명령은 서버(호스트) 창에서만 동작합니다."));
		return;
	}

	URunProgressSubsystem* Run = URunProgressSubsystem::Get(World);
	if (!Run)
	{
		UE_LOG(LogHeist, Warning, TEXT("URunProgressSubsystem 을 찾지 못했습니다."));
		return;
	}

	FGameplayTag SiteTag;
	if (Args.IsValidIndex(0))
	{
		SiteTag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), false);
		if (!SiteTag.IsValid())
		{
			return;
		}
	}
	else
	{
		SiteTag = Run->GetNextSite();
		if (!SiteTag.IsValid())
		{
			return;
		}
	}

	Run->RecordSiteCleared(SiteTag);

	if (AShelterGameState* GS = World->GetGameState<AShelterGameState>())
	{
		GS->PublishRunProgress();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRunClearSiteCommand(
	TEXT("hh.Run.Clear"),
	TEXT("hh.Run.Clear [Site.태그] - 장소 통과 처리, 인자를 비울 경우 지금 목표를 통과"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunClearSiteCommand),
	ECVF_Cheat);
