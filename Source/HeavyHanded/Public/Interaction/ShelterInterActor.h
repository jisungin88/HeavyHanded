// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h" // Tag 사용 위함
#include "GameFramework/Actor.h"
#include "ShelterInterActor.generated.h"

class AShelterPlayerController;


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
