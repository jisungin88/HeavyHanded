// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GuardAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UGuardAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	// 주변을 두리번거리는 애니메이션 상태를 변경한다.
	void SetLookAround(bool bNewLookAround);

private:

	// 현재 주변을 두리번거리는 애니메이션 상태인지 나타낸다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsLookAround = false;
	
};
