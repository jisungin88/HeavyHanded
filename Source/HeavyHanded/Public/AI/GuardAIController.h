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
	//
	// 아직 현재 목표에 도착하지 않았다면 지점을 넘기지 않고 그대로 유지한다.
	// 이 함수는 브랜치에 진입할 때마다 불리는데, 시야 획득/상실로 순찰이
	// abort 됐다 재개될 때마다 지점을 건너뛰면 순찰 경로가 망가진다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	void SelectNextPatrolPoint();

	// 이 거리(2D) 안이면 현재 순찰 지점에 도착한 것으로 본다.
	// BT 의 Move To 노드 Acceptable Radius 보다 조금 크게 잡을 것 —
	// 작으면 도착 판정이 안 나 같은 지점을 무한히 다시 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Patrol", meta = (ClampMin = "0.0", Units = "cm"))
	float PatrolArrivalRadius = 120.f;

	// 다음 수색 지점을 골라 Blackboard의 InvestigateLocation에 써넣는다.
	// Investigate 브랜치 진입 시 BTTask_SelectSearchPoint가 호출한다.
	//
	// 한 번의 조사는 [마지막 목격 지점] -> [주변 무작위 지점 x SearchSweepCount] 순서로
	// 진행된다. 더 훑을 지점이 없으면 false를 돌려주고, 호출한 태스크가 Failed 로
	// 브랜치를 끝내 순찰로 돌려보낸다.
	//
	// 조사 세션은 SearchStartTime 값으로 구분한다. 그 값이 바뀌면(= 게이지가 다시
	// 가득 찼거나 새 소음을 들었으면) 새 조사로 보고 훑기 횟수를 초기화한다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Investigate")
	bool SelectNextSearchPoint();

	// 마지막 목격 지점을 확인한 뒤 주변을 몇 번 더 훑을지.
	// 0 이면 목격 지점만 확인하고 순찰로 돌아간다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0"))
	int32 SearchSweepCount = 3;

	// 훑을 무작위 지점을 고르는 반경. NavMesh 위에서만 고른다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0.0", Units = "cm"))
	float SearchSweepRadius = 600.f;

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

	// 시야/청각 파라미터. 지역 변수로 두면 디테일 패널에 뜨지 않아
	// 반경·시야각을 전혀 조정할 수 없다(엔진 기본값 고정). 멤버로 들고 있어야
	// BP_GuardAIController 및 그 파생 BP 에서 GuardType 별로 값을 덮어쓸 수 있다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|AI|Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	// OnPossess 때 빙의한 폰에서 가져와 바인딩해 둔다. HandlePerceptionFull 에서 ResetPerception 에 쓴다
	UPROPERTY()
	TObjectPtr<UPerceptionMeterComponent> PerceptionMeter;

	// 마지막으로 선택된 순찰 지점 인덱스. 다음 호출 시 패턴에 따라 갱신.
	int32 CurrentPatrolIndex = -1;

	// PingPong 패턴에서 현재 진행 방향 (true=정방향/증가, false=역방향/감소)
	bool bPatrolMovingForward = true;

	// 진단용. 순찰 지점 선택 간격을 로그에 남겨 abort/restart 폭주를 구분한다.
	// 음수는 "아직 한 번도 고른 적 없음".
	float LastPatrolSelectTime = -1.f;

	// 진단용. 시야를 잃은 시각. 되찾을 때 상실이 몇 초 지속됐는지 찍는다.
	// 음수는 "현재 상실 상태가 아님".
	float SightLostAtTime = -1.f;

	// 이번 조사에서 지금까지 고른 지점 수. 0 = 마지막 목격 지점 자체.
	// -1 은 "이번 조사에서 아직 아무것도 고르지 않음".
	int32 CurrentSearchStep = -1;

	// 조사 세션 식별자로 쓰는 SearchStartTime 스냅샷.
	// 이 값이 Blackboard 의 것과 달라지면 새 조사가 시작된 것이다.
	float HandledSearchStartTime = TNumericLimits<float>::Lowest();
};
