// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AITypes.h"

#include "GuardSightAComponent.generated.h"


class AGuardAIController;

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

//struct FAIStimulus; //확인필요
class AActor;


///DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerSpotted, AActor*, SpottedActor);



UCLASS( ClassGroup=(AI), meta=(BlueprintSpawnableComponent) )
class HEAVYHANDED_API UGuardSightAComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGuardSightAComponent();

	//UPROPERTY(BlueprintAssignable, Category = "Guard|Perception")
	///FOnPlayerSpotted OnPlayerSpotted;


protected:

	// Called when the game starts
	virtual void BeginPlay() override;


public:

	UFUNCTION()
	void OnTargetPerceptionUpdatedSight
			(AActor* Actor, struct FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp);

	// 시야/청각 파라미터. 반경·시야각 등 실제 수치는 DT_GuardStats(FGuardStatsRow)에서
	// OnPossess 때 GuardType 에 맞는 행으로 덮어쓴다(ApplyGuardStats). 여기 생성자 기본값은
	// 테이블 조회가 실패했을 때의 폴백이며, 멤버로 들고 있어야 디테일 패널에도 노출된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GuardSight")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// 각각 디버그용 // lee
	// 시야 감지 사용 여부.
	// BP에서 Guard 종류별로 시야를 켜고 끌 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GuardSight")
	bool bEnableSight = true;


	void SetSightConfig(float InSightRadius, float InLoseSightRadius, float InPeripheralVisionAngle);

	void SetSightEnabled(bool isEnable);


private:

	// 진단용. 시야를 잃은 시각. 되찾을 때 상실이 몇 초 지속됐는지 찍는다.
	// 음수는 "현재 상실 상태가 아님".
	float SightLostAtTime = -1.f;

	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;



	/*


	public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	*/

};
