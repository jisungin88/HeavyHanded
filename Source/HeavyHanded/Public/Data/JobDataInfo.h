// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "JobDataInfo.generated.h"


UENUM(BlueprintType)
enum class ESkillType : uint8
{
	Passive UMETA(DisplayName = "Passive"),
	ActiveA UMETA(DisplayName = "ActiveA"),
	ActiveB UMETA(DisplayName = "ActiveB"),
	TeamSynergy UMETA(DisplayName = "TeamSynergy")
};


USTRUCT(BlueprintType)
struct FSkillInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillType SkillType = ESkillType::Passive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SkillName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true))
	FText SkillEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> SkillIcon = nullptr;
};

USTRUCT(BlueprintType)
struct FJobInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText JobName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true))
	FText JobDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> JobIcon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSkillInfo> Skills;
};
