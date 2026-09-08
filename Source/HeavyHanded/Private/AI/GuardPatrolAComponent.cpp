// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/GuardPatrolAComponent.h"
#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"

#include "Character/GuardCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "AITypes.h"



// Sets default values for this component's properties
UGuardPatrolAComponent::UGuardPatrolAComponent()
{

}

void UGuardPatrolAComponent::SetPatrolStats(float InArrivalRadius, int32 InSweepCount, float InSweepRadius)
{
	PatrolArrivalRadius = InArrivalRadius;
	SearchSweepCount = InSweepCount;
	SearchSweepRadius = InSweepRadius;
}


void UGuardPatrolAComponent::SelectNextPatrolPoint2()
{
	//const AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(GetPawn());
	//UBlackboardComponent* BlackboardComp = GetBlackboardComponent();


	AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(Controller))
	{
		return;
	}

	const AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(Controller->GetPawn());
	UBlackboardComponent* BlackboardComp = Controller->GetBlackboardComponent();



	if (!IsValid(GuardPawn))
	{
		// 수정 필요
		// UE_LOG(LogGuardAI, Warning,
		// 	TEXT("[%s] AGuardCharacter 가 아니라 순찰 지점을 읽을 수 없다 (현재 폰: %s)."),
		// 	*GetName(), *GetNameSafe(GetPawn()));
		return;
	}

	if (!IsValid(BlackboardComp))
	{
		// 수정 필요
		// UE_LOG(LogGuardAI, Warning, TEXT("[%s] Blackboard 가 없어 PatrolLocation 을 쓸 수 없다."), *GetName());
		return;
	}

	const int32 PointCount = GuardPawn->GetPatrolPointCount();
	if (PointCount == 0)
	{
		// 수정 필요
		//UE_LOG(LogGuardAI, Warning,
		//	TEXT("[%s] PatrolPoints 가 비어 있다. EditInstanceOnly 라 레벨에 '배치된' 액터에만 값이 붙는다 "
		//		"— 스폰된 경비라면 여기서 항상 비어 있다."),
		//	*GetNameSafe(GuardPawn));
		return;
	}

	// 아직 현재 목표에 도착하지 않았다면 지점을 넘기지 않는다.
	//
	// 이 함수는 순찰 브랜치에 진입할 때마다 호출되는데, 시야 획득으로 순찰이
	// abort 되고 상실 후 재개되는 것도 "새 진입"이다. 진입마다 전진시키면A
	// 경비가 플레이어를 한 번 볼 때마다 순찰 지점을 하나씩 건너뛴다.
	if (CurrentPatrolIndex >= 0)
	{
		FVector CurrentTarget;
		if (GuardPawn->GetPatrolLocation(CurrentPatrolIndex, CurrentTarget))
		{
			// Z 는 무시한다 - 지점 액터가 바닥에서 떠 있어도 도착 판정이 되도록.
			const float DistToCurrent = FVector::Dist2D(GuardPawn->GetActorLocation(), CurrentTarget);
			if (DistToCurrent > PatrolArrivalRadius)
			{
				// 가던 길을 계속 간다. Blackboard 값은 다시 써준다 —
				// 조사 브랜치를 거치는 동안 다른 값으로 덮였을 수 있다.
				BlackboardComp->SetValueAsVector(GuardAIKeys::PatrolLocation, CurrentTarget);
				return;
			}
		}
	}

	// 첫 호출(-1). 근처에 다른 경비가 있으면 그 경비에게서 가장 먼 지점에서, 없으면 0번에서 시작.
	if (CurrentPatrolIndex < 0)
	{
		// 수정필요(함수명)
		CurrentPatrolIndex = SelectInitialPatrolIndex2(GuardPawn);
	}
	else if (PointCount == 1)
	{
		CurrentPatrolIndex = 0;
	}
	else
	{
		switch (GuardPawn->PatrolPattern)
		{
		case EPatrolPattern::Loop:
			CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PointCount;
			break;

		case EPatrolPattern::PingPong:
			if (bPatrolMovingForward)
			{
				CurrentPatrolIndex++;
				if (CurrentPatrolIndex >= PointCount - 1)
				{
					CurrentPatrolIndex = PointCount - 1;
					bPatrolMovingForward = false; // 끝에 도달 -> 역방향으로 전환
				}
			}
			else
			{
				CurrentPatrolIndex--;
				if (CurrentPatrolIndex <= 0)
				{
					CurrentPatrolIndex = 0;
					bPatrolMovingForward = true; // 처음으로 복귀 -> 정방향으로 전환
				}
			}
			break;

		case EPatrolPattern::Random:
		{
			// 직전 지점을 제외하고 뽑아서, 같은 자리에 멈춰있는 것처럼 보이는 걸 방지.
			int32 NextIndex = CurrentPatrolIndex;
			while (NextIndex == CurrentPatrolIndex)
			{
				NextIndex = FMath::RandRange(0, PointCount - 1);
			}
			CurrentPatrolIndex = NextIndex;
		}
		break;
		}
	}

	FVector NextLocation;
	if (GuardPawn->GetPatrolLocation(CurrentPatrolIndex, NextLocation))
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::PatrolLocation, NextLocation);

		// 정상 동작이면 순찰 지점에 도착할 때마다 한 번씩만 찍힌다.
		// 호출 간격(dt)이 프레임 단위이고 폰이 제자리면 브랜치가 abort/restart 를
		// 반복하는 것이고, dt 가 수 초 단위면 실제로 걸어서 도착하고 있는 것이다.
		const float Now = GetWorld()->GetTimeSeconds();
		const float DeltaSinceLast = (LastPatrolSelectTime < 0.f) ? -1.f : (Now - LastPatrolSelectTime);
		LastPatrolSelectTime = Now;

		const FVector PawnLocation = GuardPawn->GetActorLocation();

		// UE_LOG(LogGuardAI, Log,
		// 	TEXT("[%s] 순찰 지점 %d 선택: %s | dt=%.3fs | 폰 위치 %s | 남은 거리 %.0f"),
		// 	*GetNameSafe(GuardPawn), CurrentPatrolIndex, *NextLocation.ToCompactString(),
		// 	DeltaSinceLast, *PawnLocation.ToCompactString(),
		// 	FVector::Dist(PawnLocation, NextLocation));
	}
	else
	{
		// 수정 필요
		//UE_LOG(LogGuardAI, Warning,
		//	TEXT("[%s] 순찰 지점 %d 의 위치를 얻지 못했다 (배열 항목이 비어 있는지 확인)."),
		//	*GetNameSafe(GuardPawn), CurrentPatrolIndex);
	}
}


int32 UGuardPatrolAComponent::SelectInitialPatrolIndex2(const AGuardCharacter* GuardPawn)
{
	const int32 PointCount = GuardPawn->GetPatrolPointCount();
	if (PointCount <= 1)
	{
		return 0;
	}

	// 반경 안의 이웃 경비를 자신을 포함해 모은다. 레벨에 배치된 경비 수가 적어
	// 매 OnPossess(경비당 한 번)마다 전체 순회해도 부담이 없다.
	TArray<const AGuardCharacter*> Cluster;
	Cluster.Add(GuardPawn);

	const float RadiusSq = FMath::Square(InitialPatrolSeparationRadius);
	for (TActorIterator<AGuardCharacter> It(GetWorld()); It; ++It)
	{
		AGuardCharacter* Other = *It;
		if (!IsValid(Other) || Other == GuardPawn)
		{
			continue;
		}

		if (FVector::DistSquared(GuardPawn->GetActorLocation(), Other->GetActorLocation()) <= RadiusSq)
		{
			Cluster.Add(Other);
		}
	}

	if (Cluster.Num() <= 1)
	{
		return 0;
	}

	// 두 경비가 서로를 이웃으로 보면 똑같은 반경 조건을 검사하므로 정렬 결과도 똑같이 나온다 -
	// 그래야 "내 순번"이 양쪽에서 일관되게 계산된다. GetUniqueID는 인스턴스마다 고정이라
	// 정렬 기준으로 안전하다.
	Cluster.Sort([](const AGuardCharacter& A, const AGuardCharacter& B)
		{
			return A.GetUniqueID() < B.GetUniqueID();
		});

	const int32 MyRank = Cluster.IndexOfByKey(GuardPawn);
	const int32 StartIndex = FMath::RoundToInt(static_cast<float>(MyRank) * PointCount / Cluster.Num()) % PointCount;

	// PingPong 은 bPatrolMovingForward 기본값이 true(정방향)라, 시작 지점을 마지막 인덱스로
	// 고르면 도착 직후 곧장 같은 지점을 다시 고르고서야 역방향으로 꺾인다. 미리 뒤집어 둔다.
	if (GuardPawn->PatrolPattern == EPatrolPattern::PingPong && StartIndex == PointCount - 1)
	{
		bPatrolMovingForward = false;
	}

	UE_LOG(LogGuardAI, Log,
		TEXT("[%s] %.0fcm 안에 이웃 경비 %d명이 있어(내 순번 %d/%d) %d번 지점에서 순찰을 시작한다 (기본 0번 대신)."),
		*GetNameSafe(GuardPawn), InitialPatrolSeparationRadius, Cluster.Num() - 1, MyRank, Cluster.Num(), StartIndex);

	return StartIndex;
}

bool UGuardPatrolAComponent::SelectNextSearchPoint2()
{
	//UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	//const APawn* GuardPawn = GetPawn();

	AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(Controller))
	{
		return false;
	}

	const AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(Controller->GetPawn());
	UBlackboardComponent* BlackboardComp = Controller->GetBlackboardComponent();


	if (!IsValid(BlackboardComp) || !IsValid(GuardPawn))
	{
		return false;
	}

	// 조사 지점은 LastKnownLocation(시야 전용 키)이 아니라 InvestigateLocation에서 읽는다.
	//
	// SearchStartTime을 갱신하는 곳이 세 군데다: 시야 기반 UBTService_UpdateDetectionGauge
	// (게이지 100% 도달), 소리 기반 HandlePerceptionFull(인지 게이지 100%),
	// 그리고 OnTargetPerceptionUpdated의 Hearing 분기(자극 1건). 셋 다 SearchStartTime을
	// 쓰는 바로 그 자리에서 InvestigateLocation도 같이 채워 넣으므로, 여기서 다시
	// LastKnownLocation을 읽으면 "시야로 진입한 조사"만 성립하고 소리로 들어온 조사는
	// LastKnownLocation이 비어 있어 매번 실패한다 — 실제로 경비를 한 번도 안 들켰는데
	// 소리만으로 게이지를 채우면 "마지막 목격 위치가 없어 수색을 시작할 수 없다" 로 막혔었다.
	const FVector SearchAnchor = BlackboardComp->GetValueAsVector(GuardAIKeys::InvestigateLocation);
	if (!FAISystem::IsValidLocation(SearchAnchor))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] 조사 지점이 없어 수색을 시작할 수 없다."), *GetNameSafe(GuardPawn));
		return false;
	}

	// SearchStartTime 이 바뀌었으면 새 조사다. 훑기 진행도를 초기화한다.
	const float SearchStartTime = BlackboardComp->GetValueAsFloat(GuardAIKeys::SearchStartTime);
	if (!FMath::IsNearlyEqual(SearchStartTime, HandledSearchStartTime))
	{
		HandledSearchStartTime = SearchStartTime;
		CurrentSearchStep = -1;
	}

	++CurrentSearchStep;

	// 0번째는 조사 지점 자체(마지막 목격 지점 또는 소리 지점). 여기부터 확인하는 게 자연스럽다.
	if (CurrentSearchStep == 0)
	{
		// 이미 InvestigateLocation에 들어있는 값과 같지만, Blackboard 갱신 시점을
		// 명시적으로 남겨 다른 리스너(위젯 등)가 "조사 0단계 진입"을 관찰할 수 있게 한다.
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, SearchAnchor);

		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 시작 - 조사 지점 %s"),
			*GetNameSafe(GuardPawn), *SearchAnchor.ToCompactString());
		return true;
	}

	if (CurrentSearchStep > SearchSweepCount)
	{
		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 종료 - %d개 지점을 훑었다. 순찰로 복귀."),
			*GetNameSafe(GuardPawn), SearchSweepCount);
		return false;
	}

	// 조사 지점 주변에서 실제로 도달 가능한 지점만 고른다.
	// 무작위 오프셋을 그냥 더하면 벽 너머나 NavMesh 밖이 나와 Move To 가 실패한다.
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation SweepPoint;

	if (IsValid(NavSys) && NavSys->GetRandomReachablePointInRadius(SearchAnchor, SearchSweepRadius, SweepPoint))
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, SweepPoint.Location);

		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 %d/%d - %s"),
			*GetNameSafe(GuardPawn), CurrentSearchStep, SearchSweepCount,
			*SweepPoint.Location.ToCompactString());
		return true;
	}

	UE_LOG(LogGuardAI, Warning,
		TEXT("[%s] 조사 지점 %s 반경 %.0f 안에서 도달 가능한 수색 지점을 찾지 못했다."),
		*GetNameSafe(GuardPawn), *SearchAnchor.ToCompactString(), SearchSweepRadius);
	return false;
}


// Called when the game starts
void UGuardPatrolAComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

/*
// Called every frame
void UGuardPatrolAComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//사용시 생성자에서 true
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	//PrimaryComponentTick.bCanEverTick = true;

	// ...

}
*/
