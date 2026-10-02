// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AI/GuardTypes.h"
#include "GuardAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UGuardAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	//-----------------------------------------------------
	//0929 수정
	// 기존 bool 방식은 두리번 애니메이션 종류를 구분하지 못해 비활성화한다.
	// 주변을 두리번거리는 애니메이션 상태를 변경한다.
	//void SetLookAround(bool bNewLookAround);
	//-----------------------------------------------------

	// 두리번 애니메이션 종류를 변경한다.
	void SetLookAroundType(EGuardLookAroundType NewType);

	// 체포 애니메이션 상태를 변경한다.
	void SetArresting(bool bNewArresting);

private:

	// 현재 주변을 두리번거리는 애니메이션 상태인지 나타낸다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsLookAround = false;

	// 애님 블루프린트에서 선택할 두리번 애니메이션 종류.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation", meta = (AllowPrivateAccess = "true"))
	EGuardLookAroundType LookAroundType = EGuardLookAroundType::None;

	// 현재 체포 애니메이션 상태인지 나타낸다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsArresting = false;

	
};
