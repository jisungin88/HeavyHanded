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



	
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	


private:

	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	// 디버그
	void DrawHearingDebug() const;

	FVector LastHearingLocation = FVector::ZeroVector;
	bool bHasHearingLocation = false;

public:
	void ClearHearingDebug();


};
