// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/ShelterInterActor.h"
#include "Core/PlayerControllers/ShelterPlayerController.h"


// Sets default values
AShelterInterActor::AShelterInterActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}





void AShelterInterActor::SetEntryTag(EEntryTagType NewEntry)
{

	// config/Phase.ini 참고할 것

	switch (NewEntry)
	{
	case EEntryTagType::Front:
		// GameplayTagList = (Tag = "Entry.Mansion.Front",DevComment="저택 정문 — 시야 노출 높음, 도주로 많음")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Entry.Mansion.Front"));
		break;


	case EEntryTagType::Garage:
		// GameplayTagList = (Tag = "Entry.Mansion.Garage", DevComment = "저택 지하 주차장 — 은폐 좋음, 내부 동선 김")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Entry.Mansion.Garage"));
		break;

	case EEntryTagType::Alley:
		// GameplayTagList = (Tag = "Entry.Mansion.Alley", DevComment = "저택 뒷골목 — 경비 적음, 진입 후 좁은 통로")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Entry.Mansion.Alley"));
		break;

	default:
		EntryTag = FGameplayTag();
		break;
	}


}

void AShelterInterActor::SetSiteTag(ESiteTagType NewSite)
{
	// config/Phase.ini 참고할 것

	switch (NewSite)
	{
	case ESiteTagType::Mansion:
		// GameplayTagList=(Tag="Site.Mansion",DevComment="저택 — 목표 $50,000 / 7분. 경비견, 삐걱거리는 마루")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Site.Mansion"));;
		break;

	case ESiteTagType::Museum:
		//GameplayTagList = (Tag = "Site.Museum", DevComment = "박물관 — 목표 $120,000 / 8분. 레이저 센서, 감시 카메라")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Site.Museum"));;
		break;

	case ESiteTagType::Bank:
		//GameplayTagList = (Tag = "Site.Bank", DevComment = "은행 — 목표 $250,000 / 9분. 압력판, 자동 셔터, 무장 경비")
		EntryTag = FGameplayTag::RequestGameplayTag(FName("Site.Bank"));;
		break;

	default:
		EntryTag = FGameplayTag();
		break;
	}

}


void AShelterInterActor::StartIngameTravel()
{

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AShelterPlayerController* ShelterPC = Cast<AShelterPlayerController>(PC);
	if (!ShelterPC)
	{
		return;
	}

	//ShelterPC->IngameTravel(EntryTag);
	
}

// Called when the game starts or when spawned
void AShelterInterActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AShelterInterActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

