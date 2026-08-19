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
class AGuardCharacter;

UCLASS()
class AGuardAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGuardAIController();

	// 경비 개체 종류. 스폰 시 BP_GuardVariant_* 쪽에서 설정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
	EGuardType GuardType = EGuardType::Standard;

	// 월드 경계도를 0~100 퍼센트로 읽어온다 (GameState에 붙는 UAlertComponent 게이지 기반).
	// BTDecorator_CheckWorldAlert 등이 참조.
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
	//
	// DT_GuardStats(FGuardStatsRow)에서 GuardType 에 맞는 행을 찾아 OnPossess 때 덮어쓴다.
	// 여기 초기값은 테이블 조회가 실패했을 때만 쓰이는 폴백이다 — BP 에서 직접 손대지 말 것.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Patrol", meta = (ClampMin = "0.0", Units = "cm"))
	float PatrolArrivalRadius = 120.f;

	// 순찰을 처음 시작할 때(레벨 배치 직후) 이 거리 안에 다른 경비가 있으면 0번 지점이 아니라
	// 그 이웃 무리 안에서 자기 순번에 맞춰 고르게 떨어진 지점에서 시작한다. 서로 겹치게
	// 배치된 경비들이 시작하자마자 같은 곳으로 걸어가 마주보고 지나가는 것을 막기 위함 —
	// RVO는 스쳐 지나가게는 해주지만 애초에 같은 지점으로 걸어가는 것 자체는 막지 못한다.
	//
	// MansionEvening 레벨 실측: 같은 순찰 루프를 공유하는 경비끼리 스폰 거리가 1020~1360cm였다.
	// 1000cm로는 못 잡아서(경계값보다 큼) 발동 안 한 케이스가 실제로 있었다 — 여유를 두고 1500으로.
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Patrol", meta = (ClampMin = "0.0", Units = "cm"))
	float InitialPatrolSeparationRadius = 1500.f;

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
	// DT_GuardStats 폴백값. 실제 값은 OnPossess 때 테이블에서 덮어쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0"))
	int32 SearchSweepCount = 3;

	// 훑을 무작위 지점을 고르는 반경. NavMesh 위에서만 고른다.
	// DT_GuardStats 폴백값. 실제 값은 OnPossess 때 테이블에서 덮어쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0.0", Units = "cm"))
	float SearchSweepRadius = 600.f;

	// Blackboard의 DetectionGauge(0~100)를 그대로 노출한다. 플레이어 화면의 게이지 위젯이
	// 디버그 모드 없이 이 값을 읽어가도록 하기 위한 UI용 게터.
	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	float GetDetectionGaugePercent() const;

	// 지금 이 경비가 시야로 쫓고 있는 대상이 InActor인지. 게이지 위젯이 "나를 보고 있는
	// 경비"만 골라내는 데 쓴다. CanSeeTarget이 아니라 TargetActor로 판정하는 이유는,
	// 시야를 놓친 직후에도 감소 유예(DecayGraceSeconds) 동안 게이지가 100에 머무르는데
	// 그 구간에도 "이 경비가 나를 쫓고 있다"는 표시는 계속 보여줘야 하기 때문이다.
	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	bool IsTargeting(const AActor* InActor) const;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	// 로컬 플레이어를 타겟하고 있을 때만 GetDetectionGaugePercent()를 폰의 머리 위
	// 위젯 컴포넌트(UDetectionGaugeWidget)로 밀어넣는다. BTService_UpdateDetectionGauge와
	// 같은 주기(0.1초)로 충분해 매 틱 대신 타이머로 돈다.
	void UpdateHeadGaugeWidget();

	// DT_GuardStats 폴백값. 실제 값은 OnPossess 때 테이블에서 덮어쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.01", Units = "s"))
	float HeadGaugeUpdateInterval = 0.1f;

	// GuardType 에 맞는 DT_GuardStats 행을 찾아 이동/지각/순찰/조사 수치를 일괄 적용한다.
	// 행을 못 찾으면 위 폴백값을 그대로 두고 경고만 남긴다. InPawn은 이동속도를 적용할 대상.
	void ApplyGuardStats(APawn* InPawn);

	// AI Perception(Sight+Hearing) 콜백. TargetActor / CanSeeTarget / SoundTargetActor 갱신.
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, struct FAIStimulus Stimulus);

	// UPerceptionMeterComponent::OnPerceptionFull 콜백. 인지 게이지가 100%에 도달하면
	// 마지막 소음 지점으로 조사를 시작하도록 Blackboard를 갱신하고 게이지를 리셋한다.
	UFUNCTION()
	void HandlePerceptionFull(FVector LastNoiseLocation);

	UPROPERTY(EditDefaultsOnly, Category = "Guard|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// 시야/청각 파라미터. 반경·시야각 등 실제 수치는 DT_GuardStats(FGuardStatsRow)에서
	// OnPossess 때 GuardType 에 맞는 행으로 덮어쓴다(ApplyGuardStats). 여기 생성자 기본값은
	// 테이블 조회가 실패했을 때의 폴백이며, 멤버로 들고 있어야 디테일 패널에도 노출된다.
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

	// 최초 순찰 시작 지점을 고른다. InitialPatrolSeparationRadius 안의 이웃 경비(자기 포함)를
	// 모아 안정적인 순서로 정렬한 뒤, 그 안에서 자기 순번에 비례해 지점 인덱스를 고르게
	// 분산시킨다 — "이웃에게서 가장 먼 지점"처럼 각자 계산하면, 서로 가까운 경비 둘이
	// 똑같이 "가장 먼 지점"을 골라 결국 같은 곳에서 다시 만나는 문제가 있어 이 방식을 쓴다.
	// PingPong 패턴이고 그 지점이 마지막 인덱스라면 bPatrolMovingForward도 함께 뒤집어
	// (기본값 true=정방향인 채로 두면) 도착하자마자 같은 지점을 한 번 더 고르는 걸 막는다.
	int32 SelectInitialPatrolIndex(const AGuardCharacter* GuardPawn);

	// 이번 조사에서 지금까지 고른 지점 수. 0 = 마지막 목격 지점 자체.
	// -1 은 "이번 조사에서 아직 아무것도 고르지 않음".
	int32 CurrentSearchStep = -1;

	// 조사 세션 식별자로 쓰는 SearchStartTime 스냅샷.
	// 이 값이 Blackboard 의 것과 달라지면 새 조사가 시작된 것이다.
	float HandledSearchStartTime = TNumericLimits<float>::Lowest();

	FTimerHandle HeadGaugeUpdateTimerHandle;
};
