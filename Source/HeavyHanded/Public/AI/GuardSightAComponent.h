// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AITypes.h"

#include "GuardSightAComponent.generated.h"


class AGuardAIController;

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

class AActor;
class AGuardCharacter;


///DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerSpotted, AActor*, SpottedActor);



UCLASS( ClassGroup=(AI), meta=(BlueprintSpawnableComponent) )
class HEAVYHANDED_API UGuardSightAComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGuardSightAComponent();

	void InitializeSightPerception(UAIPerceptionComponent* InPerceptionComp);

	//UPROPERTY(BlueprintAssignable, Category = "Guard|Perception")
	///FOnPlayerSpotted OnPlayerSpotted;


protected:

	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void OnRegister() override;


public:

	UFUNCTION()
	void OnTargetPerceptionUpdatedSight
			(AActor* Actor, struct FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp);

	// 시야/청각 파라미터. 반경·시야각 등 실제 수치는 DT_GuardStats(FGuardStatsRow)에서
	// OnPossess 때 GuardType 에 맞는 행으로 덮어쓴다(ApplyGuardStats). 여기 생성자 기본값은
	// 테이블 조회가 실패했을 때의 폴백이며, 멤버로 들고 있어야 디테일 패널에도 노출된다.

	// Instanced : 언리얼에게 이 프로퍼티가 가리키는 UObject도 **소유 컴포넌트별로 인스턴스화해야 한다고** 알려주는 역할
	// SightConfig UObject의 인스턴싱/복제 문제

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Instanced,  Category = "GuardSight")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;



	void SetSightEnabled(bool isEnable);

	// 경비의 전체 시야 설정을 적용한다.
	// 전체 수평 시야각은 AI Perception의 실제 시야 범위에 사용하고,
	// 양안 시야각은 이후 인지 게이지 상승 속도 보정에 사용한다.
	void SetSightConfig(float InSightRadius, float InLoseSightRadius,
		float InPeripheralVisionAngle, float InVerticalVisionAngle, float InBinocularVisionAngle);


public:

	// 대상이 현재 경비의 양안 시야 안에 있는지 확인한다.
	// 양안 시야는 전체 수평 시야각보다 좁은 중앙 영역이며,
	// 인지 게이지 상승 속도를 결정할 때 사용한다.
	bool IsWithinBinocularVisionAngle(AActor* TargetActor) const;

	// 대상이 양안 시야 안에 있는지에 따라 인지 게이지 상승 배율을 반환한다.
	// 양안 시야 안에서는 정상 속도, 주변 시야에서는 감소된 속도를 사용한다.
	float GetBinocularVisionRate(AActor* TargetActor) const;

	void SetSightDebugEnabled(bool bInEnabled)
	{
		bDrawSightDebug = bInEnabled;
		PrimaryComponentTick.bCanEverTick = bInEnabled;
	}

private:
	UPROPERTY()
	float BinocularVisionAngleDegrees = 0.0f;

	float VerticalVisionAngleDegrees = 0.0f;


	// 진단용. 시야를 잃은 시각. 되찾을 때 상실이 몇 초 지속됐는지 찍는다.
	// 음수는 "현재 상실 상태가 아님".
	float SightLostAtTime = -1.f;

	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;


	// 수직 시야 판정 함수
	bool IsWithinVerticalVisionAngle(AActor* TargetActor) const;


	bool bDrawSightDebug = true;


	//public:

	void DrawSightDebug() const;
	void DrawPerceivedActorsDebug() const;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


};
