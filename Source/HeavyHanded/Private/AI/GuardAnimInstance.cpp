// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardAnimInstance.h"

void UGuardAnimInstance::SetLookAround(bool bNewLookAround)
{
	// Wait 노드의 실행 상태에 맞춰 두리번거리기 상태를 변경한다.
	bIsLookAround = bNewLookAround;

	//UE_LOG(LogTemp, Warning, TEXT("[LookAround] bIsLookAround = %s"),
	//	bIsLookAround ? TEXT("TRUE") : TEXT("FALSE"));
}

void UGuardAnimInstance::SetArresting(bool bNewArresting)
{
	// 체포 Task의 실행 상태에 맞춰 체포 애니메이션 상태를 변경한다.
	bIsArresting = bNewArresting;

	UE_LOG(LogTemp, Warning, TEXT("[Arrest] bIsArresting = %s"),
		bIsArresting ? TEXT("TRUE") : TEXT("FALSE"));
}
