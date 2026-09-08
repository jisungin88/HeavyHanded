// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AITypes.h"

#include "GuardHearingAComponent.generated.h"



class UAIPerceptionComponent;
class UAISenseConfig_Hearing;

UCLASS( ClassGroup=(AI), meta=(BlueprintSpawnableComponent) )
class HEAVYHANDED_API UGuardHearingAComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGuardHearingAComponent();

	void InitializeHearingPerception(UAIPerceptionComponent* InPerceptionComp);



	UFUNCTION()
	void OnTargetPerceptionUpdatedHearing
	(AActor* Actor, struct FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp);

	void SetHearingRange(float InHearingRange);

	void SetHearingEnabled(bool isEnable);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GuardHearing")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;


	// 각각 디버그용 // lee
	// 청각 감지 사용 여부.
	// BP에서 Guard 종류별로 청각을 켜고 끌 수 있다.
	// UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GuardHearing")
	// bool bEnableHearing = true;


	/*
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	*/


private:

	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;
		
};
