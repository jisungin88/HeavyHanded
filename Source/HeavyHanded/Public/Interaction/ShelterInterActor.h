// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h" // Tag 사용 위함
#include "GameFramework/Actor.h"
#include "ShelterInterActor.generated.h"

class AShelterPlayerController;


/**
 * [사용하지 않음 — 2026-09-17] 진입점·목표 선택은 AShelterPlayerController 의 Server RPC 와
 * AShelterGameState 로 옮김. 이 클래스를 새로 상속하거나 배치하지 말 것.
 * 남겨 둔 것은 초기 선택 방식의 기록을 위해서다.
 */

UENUM(BlueprintType)
enum class EEntryTagType : uint8
{
	None,
	Front,
	Garage,
	Alley
};

UENUM(BlueprintType)
enum class ESiteTagType : uint8
{
	None,
	Mansion,
	Museum,
	Bank
};


UCLASS()
class HEAVYHANDED_API AShelterInterActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AShelterInterActor();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
	FGameplayTag EntryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
	FGameplayTag SiteTag;



	UFUNCTION(BlueprintCallable, Category = "Travel")
	void SetEntryTag(EEntryTagType NewEntry);

	UFUNCTION(BlueprintCallable, Category = "Travel")
	void SetSiteTag(ESiteTagType NewSite);

	UFUNCTION(BlueprintCallable, Category = "Travel")
	void StartIngameTravel();


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
