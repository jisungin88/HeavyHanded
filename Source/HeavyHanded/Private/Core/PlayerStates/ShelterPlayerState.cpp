// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Core/GameStates/ShelterGameState.h"
#include "Net/UnrealNetwork.h"


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

	// 서버에서는 RepNotify가 자동 호출되지 않기 때문에
	// 서버에서도 UI 갱신이 필요하다면 직접 호출
	OnSelectedJobChanged.Broadcast(this);


	AShelterGameState* GS = GetWorld()->GetGameState<AShelterGameState>();
	if (GS)
	{
		GS->UpdateCanStart();
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

void AShelterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// SelectedJob을 모든 클라이언트에 복제
	DOREPLIFETIME(AShelterPlayerState,SelectedJob);
}

