// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/PlayerStates/ShelterPlayerState.h"
#include "Net/UnrealNetwork.h"



void AShelterPlayerState::SetSelectedJob(EJobType NewJob)
{
	// 직업 변경은 서버에서만 가능
	if (!HasAuthority())
	{
		return;
	}

	// 새로운 직업 저장
	SelectedJob = NewJob;

	// 서버에서는 RepNotify가 자동 호출되지 않기 때문에
	// 서버에서도 UI 갱신이 필요하다면 직접 호출
	OnRep_SelectedJob();
}

void AShelterPlayerState::OnRep_SelectedJob()
{
	UE_LOG(LogTemp, Warning, TEXT("SelectedJob Replicated: %d"),
		static_cast<int32>(SelectedJob));
	// 직업 UI 갱신 이벤트 등을 연결
	OnSelectedJobChanged.Broadcast();
}

void AShelterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// SelectedJob을 모든 클라이언트에 복제
	DOREPLIFETIME(AShelterPlayerState,SelectedJob);
}
