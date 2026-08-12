#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/GuardTypes.h"
#include "GuardAIController.generated.h"

class UBehaviorTree;
class UBlackboardComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UPerceptionMeterComponent;
class AActor;

UCLASS()
class AGuardAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGuardAIController();

	// 경비 개체 종류. 스폰 시 BP_GuardVariant_* 쪽에서 설정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
	EGuardType GuardType = EGuardType::Standard;

	// 월드 경계도(임시 GameState 변수)를 읽어온다. Decorator/Service가 참조.
	// TODO: 정식 UAlertComponent로 교체 시 이 함수 내부만 수정하면 된다.
	float GetWorldAlertLevel() const;

	// 다음 순찰 지점을 골라 Blackboard의 PatrolLocation에 써넣는다.
	// Patrol 브랜치 진입 시 BTTask_SelectNextPatrolPoint가 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	void SelectNextPatrolPoint();

protected:
	virtual void OnPossess(APawn* InPawn) override;

	// AI Perception(Sight+Hearing) 콜백. TargetActor / CanSeeTarget / SoundTargetActor 갱신.
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, struct FAIStimulus Stimulus);

	// UPerceptionMeterComponent::OnPerceptionFull 콜백. 인지 게이지가 100%에 도달하면
	// 마지막 소음 지점으로 조사를 시작하도록 Blackboard를 갱신하고 게이지를 리셋한다.
	UFUNCTION()
	void HandlePerceptionFull(FVector LastNoiseLocation);

	UPROPERTY(EditDefaultsOnly, Category = "Guard|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

private:
	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	// OnPossess 때 빙의한 폰에서 가져와 바인딩해 둔다. HandlePerceptionFull 에서 ResetPerception 에 쓴다
	UPROPERTY()
	TObjectPtr<UPerceptionMeterComponent> PerceptionMeter;

	// 마지막으로 선택된 순찰 지점 인덱스. 다음 호출 시 패턴에 따라 갱신.
	int32 CurrentPatrolIndex = -1;

	// PingPong 패턴에서 현재 진행 방향 (true=정방향/증가, false=역방향/감소)
	bool bPatrolMovingForward = true;
};
