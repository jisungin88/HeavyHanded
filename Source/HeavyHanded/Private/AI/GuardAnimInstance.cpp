// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardAnimInstance.h"
#include "Character/GuardCharacter.h"

void UGuardAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	// 복제 도착 당시 AnimInstance가 없었거나 재생성되어도 현재 상태를 적용한다.
	if (const AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(TryGetPawnOwner()); IsValid(GuardCharacter))
	{
		SetLookAroundType(GuardCharacter->GetLookAroundType());
		SetArresting(GuardCharacter->IsArresting());
	}
}

/*
void UGuardAnimInstance::SetLookAround(bool bNewLookAround)
{
	// Wait 노드의 실행 상태에 맞춰 두리번거리기 상태를 변경한다.
	bIsLookAround = bNewLookAround;

	//UE_LOG(LogTemp, Warning, TEXT("[LookAround] bIsLookAround = %s"),
	//	bIsLookAround ? TEXT("TRUE") : TEXT("FALSE"));
}
*/

void UGuardAnimInstance::SetLookAroundType(EGuardLookAroundType NewType)
{
	LookAroundType = NewType;
	bIsLookAround = (NewType != EGuardLookAroundType::None);
}

void UGuardAnimInstance::SetArresting(bool bNewArresting)
{
	if (bIsArresting == bNewArresting)
	{
		return;
	}
	// 체포 Task의 실행 상태에 맞춰 체포 애니메이션 상태를 변경한다.
	bIsArresting = bNewArresting;

	UE_LOG(LogTemp, Warning, TEXT("[Arrest] bIsArresting = %s"),
		bIsArresting ? TEXT("TRUE") : TEXT("FALSE"));
}
