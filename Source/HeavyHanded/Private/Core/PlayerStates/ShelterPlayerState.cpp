// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Net/UnrealNetwork.h"


EJobType JobTypeFromRoleTag(const FGameplayTag& RoleTag)
{
	const FName Name = RoleTag.GetTagName();
	if (Name == "Role.Brute")  return EJobType::Brute;
	if (Name == "Role.Ghost")  return EJobType::Ghost;
	if (Name == "Role.Oracle") return EJobType::Oracle;
	if (Name == "Role.Mimic")  return EJobType::Mimic;
	return EJobType::None;
}

FGameplayTag RoleTagFromJobType(EJobType Job)
{
	switch (Job)
	{
	case EJobType::Brute: 	return FGameplayTag::RequestGameplayTag("Role.Brute");
	case EJobType::Ghost: 	return FGameplayTag::RequestGameplayTag("Role.Ghost");
	case EJobType::Oracle: 	return FGameplayTag::RequestGameplayTag("Role.Oracle");
	case EJobType::Mimic: 	return FGameplayTag::RequestGameplayTag("Role.Mimic");
	default:				return FGameplayTag();
	}
}


void AShelterPlayerState::SetSelectedJob(EJobType NewJob)
{
	// 직업 변경은 서버에서만 가능
	if (!HasAuthority())
	{
		return;
	}


	if (GEngine)
	{
		FString Message = FString::Printf(
			TEXT("[3 PS SERVER] PS=%p / %s / %s -> %s"),
			this,
			*GetName(),
			*UEnum::GetValueAsString(SelectedJob),
			*UEnum::GetValueAsString(NewJob)
		);

		GEngine->AddOnScreenDebugMessage(
			-1, 10.f, FColor::Yellow, Message
		);


		FString Message2 = FString::Printf(TEXT("[server] 3. PS : %s -> SelectJob 호출 / %s -> %s"),
			*GetName(), *UEnum::GetValueAsString(SelectedJob), *UEnum::GetValueAsString(NewJob));
		//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, Message2);

	}


	// 새로운 직업 저장
	SelectedJob = NewJob;

	// 직업이 풀렸으면 확정도 같이 풀린다 (ClearJob / 퇴장 경로).
	// 여기서 같이 내리지 않으면 확정 플래그만 살아남아 선택 화면을 건너뛴다
	if (NewJob == EJobType::None)
	{
		SetJobConfirmed(false);
	}

	// 서버에서는 RepNotify가 자동 호출되지 않기 때문에
	// 서버에서도 UI 갱신이 필요하다면 직접 호출
	OnSelectedJobChanged.Broadcast(this);


	AShelterGameState* GS = GetWorld()->GetGameState<AShelterGameState>();
	if (GS)
	{
		GS->UpdateCanStart();
	}

}


void AShelterPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();

	if (const UWorld* World = GetWorld())
	{
		if (AShelterGameState* GS = World->GetGameState<AShelterGameState>())
		{
			GS->NotifyRosterDirty();
		}
	}
}

void AShelterPlayerState::OnRep_SelectedJob()
{
	FString JobName = UEnum::GetValueAsString(SelectedJob);

	//if (GEngine)
	//{
	//	//FString Message = FString::Printf(TEXT("4. PS -> SelectedJob Replicated: %s - OnRep_SelectedJob 호출"), *JobName);
	//	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, Message);
	//	//if (OnSelectedJobChanged.IsBound())
	//	//{
	//	//	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("델리게이트 바인딩됨"));
	//	//}
	//	//else
	//	//{
	//	//	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, TEXT("델리게이트 바인딩 안됨"));
	//	//}
	//	//FString Message2 = FString::Printf(TEXT("4. PS -> OnRep_SelectedJob 호출[%s]OnRep PS: %s / Bound: %s"),
	//		//HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), *GetName(), OnSelectedJobChanged.IsBound() ? TEXT("TRUE") : TEXT("FALSE"));
	//	//GEngine->AddOnScreenDebugMessage(-1, 10.0f, OnSelectedJobChanged.IsBound() ? FColor::Green : FColor::Red, Message2);
	//}


	FString Message = FString::Printf(TEXT("[4 OnRep] PS=%p / %s / Authority=%s / Job=%s"), this, *GetName(), HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"), *UEnum::GetValueAsString(SelectedJob));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, Message);
	}



	// 직업 UI 갱신 이벤트 등을 연결
	OnSelectedJobChanged.Broadcast(this);
}

void AShelterPlayerState::SetJobConfirmed(bool bNewConfirmed)
{
	// 확정 판정은 서버에서만
	if (!HasAuthority())
	{
		return;
	}

	if (bJobConfirmed == bNewConfirmed)
	{
		return;
	}

	bJobConfirmed = bNewConfirmed;

	// 서버에서는 RepNotify가 자동 호출되지 않으므로 호스트 UI를 위해 직접 알린다
	OnJobConfirmedChanged.Broadcast(this);
}

void AShelterPlayerState::OnRep_JobConfirmed()
{
	OnJobConfirmedChanged.Broadcast(this);
}

void AShelterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// SelectedJob을 모든 클라이언트에 복제
	DOREPLIFETIME(AShelterPlayerState,SelectedJob);
	DOREPLIFETIME(AShelterPlayerState,bJobConfirmed);

	DOREPLIFETIME_CONDITION(AShelterPlayerState, NicknameFeedback, COND_OwnerOnly);
}

void AShelterPlayerState::ReportNicknameError(ENicknameError Error)
{
	if (!HasAuthority())
	{
		return;
	}

	NicknameFeedback.Error = Error;

	// 같은 사유로 거절이 되었을 때 값이 안바뀌면 복제가 일어나지 않음
	// 일련번호를 반영하여 두 번째 거절에서도 화면이 뜨게 함
	++NicknameFeedback.Seq;

	OnNicknameRejected.Broadcast(Error);
}

void AShelterPlayerState::OnRep_NicknameFeedback()
{
	OnNicknameRejected.Broadcast(NicknameFeedback.Error);
}

