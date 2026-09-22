// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AITypes.h"

#include "GuardHearingAComponent.generated.h"



class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class AGuardCharacter;
class AGuardAIController;

UCLASS( ClassGroup=(AI), meta=(BlueprintSpawnableComponent) )
class HEAVYHANDED_API UGuardHearingAComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGuardHearingAComponent();

	void InitializeHearingPerception(UAIPerceptionComponent* InPerceptionComp);

public:
	void Initialize(AGuardCharacter* InGuardCharacter, UAIPerceptionComponent* InPerceptionComp);


	UFUNCTION()
	void OnTargetPerceptionUpdatedHearing
	(AActor* Actor, struct FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp);

	void SetHearingRange(float InHearingRange);

	void SetHearingEnabled(bool isEnable);

public:
	UFUNCTION() // 타이머 만료 함수
		void HandleWorldAlertSilenceTimeout();

	UFUNCTION() // 디버그용
		void LogWorldAlertSilenceRemaining();

	void StartWorldAlertSilenceTimer();
	void ClearWorldAlertSilenceTimer();


protected:
	// Called when the game starts
	virtual void BeginPlay() override;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GuardHearing")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;



	
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	


private:
	// 경계도 속도 증가 상태를 해제하기 위한 무소음 타이머.
	FTimerHandle WorldAlertSilenceTimerHandle;

	// 무소음 타이머의 남은 시간을 1초마다 디버그 출력한다.
	FTimerHandle WorldAlertSilenceDebugTimerHandle;


	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	// 디버그
	void DrawHearingDebug() const;

	FVector LastHearingLocation = FVector::ZeroVector;
	bool bHasHearingLocation = false;

public:
	void ClearHearingDebug();


private:

	UPROPERTY(Transient)
	TObjectPtr<AGuardAIController> GuardAIController;

	UPROPERTY(Transient)
	TObjectPtr<AGuardCharacter> GuardCharacter;

};
