// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GuardPatrolAComponent.generated.h"


class AGuardCharacter;
class UBlackboardComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HEAVYHANDED_API UGuardPatrolAComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGuardPatrolAComponent();

public:
	void SetPatrolStats(float InArrivalRadius, int32 InSweepCount, float InSweepRadius);
	

	// ========================================================
	// Patrol (순찰)
	// ========================================================

public:

	// 다음 순찰 지점을 골라 Blackboard의 PatrolLocation에 써넣는다.
	// Patrol 브랜치 진입 시 BTTask_SelectNextPatrolPoint가 호출한다.
	//
	// 아직 현재 목표에 도착하지 않았다면 지점을 넘기지 않고 그대로 유지한다.
	// 이 함수는 브랜치에 진입할 때마다 불리는데, 시야 획득/상실로 순찰이
	// abort 됐다 재개될 때마다 지점을 건너뛰면 순찰 경로가 망가진다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	void SelectNextPatrolPoint2();


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


protected:

	// 최초 순찰 시작 지점을 고른다. InitialPatrolSeparationRadius 안의 이웃 경비(자기 포함)를
	// 모아 안정적인 순서로 정렬한 뒤, 그 안에서 자기 순번에 비례해 지점 인덱스를 고르게
	// 분산시킨다 — "이웃에게서 가장 먼 지점"처럼 각자 계산하면, 서로 가까운 경비 둘이
	// 똑같이 "가장 먼 지점"을 골라 결국 같은 곳에서 다시 만나는 문제가 있어 이 방식을 쓴다.
	// PingPong 패턴이고 그 지점이 마지막 인덱스라면 bPatrolMovingForward도 함께 뒤집어
	// (기본값 true=정방향인 채로 두면) 도착하자마자 같은 지점을 한 번 더 고르는 걸 막는다.
	int32 SelectInitialPatrolIndex2(const AGuardCharacter* GuardPawn);



	// ========================================================
	// Investigate (수색)
	// ========================================================

public:

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
	bool SelectNextSearchPoint2();


	// 마지막 목격 지점을 확인한 뒤 주변을 몇 번 더 훑을지.
	// 0 이면 목격 지점만 확인하고 순찰로 돌아간다.
	// DT_GuardStats 폴백값. 실제 값은 OnPossess 때 테이블에서 덮어쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0"))
	int32 SearchSweepCount = 3;


	// 훑을 무작위 지점을 고르는 반경. NavMesh 위에서만 고른다.
	// DT_GuardStats 폴백값. 실제 값은 OnPossess 때 테이블에서 덮어쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Investigate", meta = (ClampMin = "0.0", Units = "cm"))
	float SearchSweepRadius = 600.f;


	



protected:
	// Called when the game starts
	virtual void BeginPlay() override;


private:

		// 마지막으로 선택된 순찰 지점 인덱스. 다음 호출 시 패턴에 따라 갱신.
		int32 CurrentPatrolIndex = -1;

		// PingPong 패턴에서 현재 진행 방향 (true=정방향/증가, false=역방향/감소)
		bool bPatrolMovingForward = true;

		// 진단용. 순찰 지점 선택 간격을 로그에 남겨 abort/restart 폭주를 구분한다.
		// 음수는 "아직 한 번도 고른 적 없음".
		float LastPatrolSelectTime = -1.f;


		// 이번 조사에서 지금까지 고른 지점 수. 0 = 마지막 목격 지점 자체.
		// -1 은 "이번 조사에서 아직 아무것도 고르지 않음".
		int32 CurrentSearchStep = -1;


		// 조사 세션 식별자로 쓰는 SearchStartTime 스냅샷.
		// 이 값이 Blackboard 의 것과 달라지면 새 조사가 시작된 것이다.
		float HandledSearchStartTime = TNumericLimits<float>::Lowest();


//public:	
//	// Called every frame
//	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
