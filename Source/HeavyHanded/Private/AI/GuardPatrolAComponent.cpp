// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/GuardPatrolAComponent.h"
#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"

#include "Character/GuardCharacter.h"
#include "Components/CapsuleComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
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


bool UGuardPatrolAComponent::SelectNextPatrolPoint2()
{
	AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(Controller) || !Controller->HasAuthority())
	{
		return false;
	}

	const AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(Controller->GetPawn());
	UBlackboardComponent* BlackboardComp = Controller->GetBlackboardComponent();
	if (!IsValid(GuardPawn) || !IsValid(BlackboardComp))
	{
		return false;
	}

	const int32 PointCount = GuardPawn->GetPatrolPointCount();
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (PointCount <= 0 || !IsValid(NavSys))
	{
		DisplayPatrolFailure(TEXT("순찰 지점 또는 NavigationSystem이 없어 순찰 지점을 선택할 수 없습니다."));
		BlackboardComp->ClearValue(GuardAIKeys::PatrolLocation);
		return false;
	}

	const ANavigationData* NavData = NavSys->GetNavDataForProps(Controller->GetNavAgentPropertiesRef(), Controller->GetNavAgentLocation());
	if (!IsValid(NavData))
	{
		DisplayPatrolFailure(TEXT("경비에 맞는 NavMesh가 없어 순찰 지점을 선택할 수 없습니다."));
		BlackboardComp->ClearValue(GuardAIKeys::PatrolLocation);
		return false;
	}

	const auto FindReachableLocation = [&](int32 Index, FVector& OutLocation)
	{
		FVector PointLocation;
		if (!GuardPawn->GetPatrolLocation(Index, PointLocation))
		{
			DisplayPatrolFailure(FString::Printf(TEXT("순찰 지점 %d 위치가 유효하지 않음 → 다음 지점 탐색"), Index));
			return false;
		}

		FNavLocation ProjectedLocation;
		if (!NavSys->ProjectPointToNavigation(PointLocation, ProjectedLocation, NavData->GetDefaultQueryExtent(), NavData))
		{
			DisplayPatrolFailure(FString::Printf(TEXT("순찰 지점 %d NavMesh 밖 → 다음 지점 탐색"), Index));
			return false;
		}

		const FSharedConstNavQueryFilter Filter = UNavigationQueryFilter::GetQueryFilter(*NavData, Controller, Controller->GetDefaultNavigationFilterClass());
		FPathFindingQuery Query(Controller, *NavData, Controller->GetNavAgentLocation(), ProjectedLocation.Location, Filter);
		Query.SetAllowPartialPaths(false);
		const FPathFindingResult Result = NavSys->FindPathSync(Controller->GetNavAgentPropertiesRef(), Query);
		if (!Result.IsSuccessful() || !Result.Path.IsValid() || Result.Path->IsPartial())
		{
			DisplayPatrolFailure(FString::Printf(TEXT("순찰 지점 %d 도달 불가 → 다음 지점 탐색"), Index));
			return false;
		}

		OutLocation = ProjectedLocation.Location;
		return true;
	};

	// 추격으로 중단된 경우에는 도달 가능한 기존 순찰 지점을 유지한다.
	FVector CurrentLocation;
	if (!bSkipCurrentPatrolPoint && CurrentPatrolIndex >= 0 && CurrentPatrolIndex < PointCount && FindReachableLocation(CurrentPatrolIndex, CurrentLocation))
	{
		if (FVector::Dist2D(GuardPawn->GetActorLocation(), CurrentLocation) > PatrolArrivalRadius)
		{
			BlackboardComp->SetValueAsVector(GuardAIKeys::PatrolLocation, CurrentLocation);
			return true;
		}
	}
	const int32 FailedIndex = bSkipCurrentPatrolPoint ? CurrentPatrolIndex : INDEX_NONE;
	bSkipCurrentPatrolPoint = false;

	TArray<int32> Candidates;
	TArray<bool> CandidateDirections;
	if (CurrentPatrolIndex < 0 || CurrentPatrolIndex >= PointCount)
	{
		CurrentPatrolIndex = SelectInitialPatrolIndex2(GuardPawn);
		Candidates.Add(CurrentPatrolIndex);
		CandidateDirections.Add(bPatrolMovingForward);
	}

	if (GuardPawn->PatrolPattern == EPatrolPattern::Random)
	{
		TArray<int32> RemainingIndices;
		for (int32 Index = 0; Index < PointCount; ++Index)
		{
			if (Index != CurrentPatrolIndex)
			{
				RemainingIndices.Add(Index);
			}
		}
		while (!RemainingIndices.IsEmpty())
		{
			const int32 RandomIndex = FMath::RandRange(0, RemainingIndices.Num() - 1);
			Candidates.Add(RemainingIndices[RandomIndex]);
			CandidateDirections.Add(bPatrolMovingForward);
			RemainingIndices.RemoveAtSwap(RandomIndex);
		}
		if (!Candidates.Contains(CurrentPatrolIndex))
		{
			Candidates.Add(CurrentPatrolIndex);
			CandidateDirections.Add(bPatrolMovingForward);
		}
	}
	else
	{
		// 왕복은 반대편 끝까지 검사해야 하므로 최대 두 배의 인덱스를 진행한다.
		const int32 MaxSteps = GuardPawn->PatrolPattern == EPatrolPattern::PingPong ? PointCount * 2 : PointCount;
		for (int32 Step = 0; Step < MaxSteps; ++Step)
		{
			if (PointCount == 1)
			{
				CurrentPatrolIndex = 0;
			}
			else if (GuardPawn->PatrolPattern == EPatrolPattern::PingPong)
			{
				if (CurrentPatrolIndex == PointCount - 1)
				{
					bPatrolMovingForward = false;
				}
				else if (CurrentPatrolIndex == 0)
				{
					bPatrolMovingForward = true;
				}
				CurrentPatrolIndex += bPatrolMovingForward ? 1 : -1;
			}
			else
			{
				CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PointCount;
			}
			if (!Candidates.Contains(CurrentPatrolIndex))
			{
				Candidates.Add(CurrentPatrolIndex);
				CandidateDirections.Add(bPatrolMovingForward);
			}
		}
	}

	for (int32 Index : Candidates)
	{
		if (Index == FailedIndex)
		{
			continue;
		}
		FVector NextLocation;
		if (!FindReachableLocation(Index, NextLocation))
		{
			continue;
		}
		CurrentPatrolIndex = Index;
		if (GuardPawn->PatrolPattern == EPatrolPattern::PingPong)
		{
			bPatrolMovingForward = CandidateDirections[Candidates.IndexOfByKey(Index)];
		}
		BlackboardComp->SetValueAsVector(GuardAIKeys::PatrolLocation, NextLocation);
		LastPatrolSelectTime = GetWorld()->GetTimeSeconds();
		return true;
	}

	BlackboardComp->ClearValue(GuardAIKeys::PatrolLocation);
	UE_LOG(LogGuardAI, Warning, TEXT("[%s] 도달 가능한 순찰 지점이 없습니다."), *GetNameSafe(GuardPawn));
	DisplayPatrolFailure(TEXT("도달 가능한 순찰 지점이 없습니다."));
	return false;
}

void UGuardPatrolAComponent::SkipCurrentPatrolPoint()
{
	const AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (IsValid(Controller) && Controller->HasAuthority())
	{
		bSkipCurrentPatrolPoint = true;
		DisplayPatrolFailure(FString::Printf(TEXT("순찰 지점 %d 이동 실패 → 다음 지점 탐색"), CurrentPatrolIndex));
	}
}

void UGuardPatrolAComponent::DisplayPatrolFailure(const FString& Message) const
{
	const AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(GEngine))
	{
		return;
	}

	// 경비별 고정 키를 사용해 반복 실패 메시지가 화면에 계속 쌓이지 않도록 한다.
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 5.0f, FColor::Red, FString::Printf(TEXT("[%s] %s"), *GetNameSafe(Controller->GetPawn()), *Message));
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

void UGuardPatrolAComponent::UpdateLastChaseDirection(const FVector& TargetLocation)
{
	const AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(Controller->GetPawn()))
	{
		return;
	}
	const FVector Direction = (TargetLocation - Controller->GetPawn()->GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		LastChaseDirection = Direction;
	}
}

bool UGuardPatrolAComponent::FindReachableSearchLocation(const FVector& DesiredLocation, FVector& OutLocation, bool bPreferExactLocation)
{
	AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(Controller) || !Controller->HasAuthority() || !FAISystem::IsValidLocation(DesiredLocation))
	{
		return false;
	}

	const AGuardCharacter* GuardPawn = Cast<AGuardCharacter>(Controller->GetPawn());
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!IsValid(GuardPawn) || !IsValid(NavSys))
	{
		Controller->LogSearchTransitionDebug(TEXT("SearchNavigationMissing"), TEXT("경비 Pawn 또는 NavigationSystem 없음"));
		return false;
	}

	ANavigationData* NavData = NavSys->GetNavDataForProps(Controller->GetNavAgentPropertiesRef(), Controller->GetNavAgentLocation());
	if (!IsValid(NavData))
	{
		Controller->LogSearchTransitionDebug(TEXT("SearchNavigationMissing"), TEXT("경비에 맞는 NavMesh 없음"));
		return false;
	}

	const FSharedConstNavQueryFilter Filter = UNavigationQueryFilter::GetQueryFilter(*NavData, Controller, Controller->GetDefaultNavigationFilterClass());
	const auto HasCompletePath = [&](const FVector& Location)
	{
		FPathFindingQuery Query(Controller, *NavData, Controller->GetNavAgentLocation(), Location, Filter);
		Query.SetAllowPartialPaths(false);
		const FPathFindingResult Result = NavSys->FindPathSync(Controller->GetNavAgentPropertiesRef(), Query);
		return Result.IsSuccessful() && Result.Path.IsValid() && !Result.Path->IsPartial();
	};

	// 목격 좌표는 캐릭터 중심 높이일 수 있으므로 캡슐 높이까지 바닥 보정을 허용한다.
	FVector ProjectionExtent = NavData->GetDefaultQueryExtent();
	const UCapsuleComponent* Capsule = GuardPawn->GetCapsuleComponent();
	if (IsValid(Capsule))
	{
		ProjectionExtent.Z = FMath::Max(ProjectionExtent.Z, static_cast<FVector::FReal>(Capsule->GetScaledCapsuleHalfHeight() * 2.0f));
	}
	FNavLocation ProjectedLocation;
	const bool bProjected = NavSys->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, ProjectionExtent, NavData, Filter);
	if (bPreferExactLocation && bProjected && HasCompletePath(ProjectedLocation.Location))
	{
		OutLocation = ProjectedLocation.Location;
		if (!OutLocation.Equals(DesiredLocation, 1.0f))
		{
			Controller->LogSearchTransitionDebug(TEXT("SearchGoalProjected"), FString::Printf(TEXT("NavMesh 보정: Original=%s Goal=%s"), *DesiredLocation.ToCompactString(), *OutLocation.ToCompactString()));
		}
		return true;
	}

	// 후보가 NavMesh 위에 있어도 경비와 다른 섬일 수 있으므로 경비 출발점에서 다시 검사한다.
	const FVector Anchor = bProjected ? ProjectedLocation.Location : DesiredLocation;
	const FVector GuardLocation = Controller->GetNavAgentLocation();
	FVector SearchDirection = LastChaseDirection;
	if (SearchDirection.IsNearlyZero())
	{
		SearchDirection = (Anchor - GuardLocation).GetSafeNormal2D();
		if (SearchDirection.IsNearlyZero())
		{
			SearchDirection = GuardPawn->GetActorForwardVector().GetSafeNormal2D();
		}
	}

	TArray<FVector> Candidates;
	const auto AddCandidate = [&](const FVector& Location)
	{
		if (!Candidates.ContainsByPredicate([&](const FVector& Existing) { return Existing.Equals(Location, 10.0f); }))
		{
			Candidates.Add(Location);
		}
	};
	// 무작위 표본에 전방 후보가 빠지는 것을 막기 위해 방향별 후보를 먼저 만든다.
	constexpr float Angles[] = { 0.0f, 45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f, 180.0f };
	constexpr float RadiusScales[] = { 0.35f, 0.7f };
	for (float RadiusScale : RadiusScales)
	{
		for (float Angle : Angles)
		{
			const FVector Seed = Anchor + SearchDirection.RotateAngleAxis(Angle, FVector::UpVector) * SearchSweepRadius * RadiusScale;
			FNavLocation Candidate;
			if (NavSys->ProjectPointToNavigation(Seed, Candidate, ProjectionExtent, NavData, Filter) && FVector::Dist2D(Anchor, Candidate.Location) <= SearchSweepRadius)
			{
				AddCandidate(Candidate.Location);
			}
		}
	}
	constexpr int32 MaxCandidateAttempts = 8;
	for (int32 Attempt = 0; Attempt < MaxCandidateAttempts; ++Attempt)
	{
		FNavLocation Candidate;
		if (NavSys->GetRandomPointInNavigableRadius(Anchor, SearchSweepRadius, Candidate, NavData, Filter))
		{
			AddCandidate(Candidate.Location);
		}
	}

	const auto GetDirectionPriority = [&](const FVector& Location)
	{
		const float Dot = FVector::DotProduct((Location - GuardLocation).GetSafeNormal2D(), SearchDirection);
		return Dot >= 0.5f ? 0 : (Dot >= -0.5f ? 1 : 2);
	};
	Candidates.Sort([&](const FVector& A, const FVector& B)
	{
		const int32 PriorityA = GetDirectionPriority(A);
		const int32 PriorityB = GetDirectionPriority(B);
		return PriorityA != PriorityB ? PriorityA < PriorityB : FVector::DistSquared2D(A, Anchor) < FVector::DistSquared2D(B, Anchor);
	});
	for (const FVector& Candidate : Candidates)
	{
		if (HasCompletePath(Candidate))
		{
			OutLocation = Candidate;
			const TCHAR* DirectionName = GetDirectionPriority(Candidate) == 0 ? TEXT("Forward") : (GetDirectionPriority(Candidate) == 1 ? TEXT("Side") : TEXT("Back"));
			Controller->LogSearchTransitionDebug(TEXT("SearchAlternativeGoal"), FString::Printf(TEXT("방향 우선 대체 지점: Sector=%s Original=%s Goal=%s ChaseDirection=%s Guard=%s"), DirectionName, *DesiredLocation.ToCompactString(), *OutLocation.ToCompactString(), *SearchDirection.ToCompactString(), *GuardLocation.ToCompactString()));
			return true;
		}
	}

	Controller->LogSearchTransitionDebug(TEXT("SearchNoReachableGoal"), FString::Printf(TEXT("원래 지점과 주변 후보 %d개에서 완전한 경로를 찾지 못함: Original=%s Radius=%.1f"), Candidates.Num(), *DesiredLocation.ToCompactString(), SearchSweepRadius));
	return false;
}

bool UGuardPatrolAComponent::SelectNextSearchPoint2()
{
	//UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	//const APawn* GuardPawn = GetPawn();

	AGuardAIController* Controller = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(Controller) || !Controller->HasAuthority())
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
		Controller->LogSearchTransitionDebug(TEXT("SearchSelectFailed"), TEXT("조사 위치가 유효하지 않음"));
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
		FVector ReachableLocation;
		if (!FindReachableSearchLocation(SearchAnchor, ReachableLocation))
		{
			return false;
		}
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, ReachableLocation);

		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 시작 - 조사 지점 %s"),
			*GetNameSafe(GuardPawn), *ReachableLocation.ToCompactString());
		return true;
	}

	if (CurrentSearchStep > SearchSweepCount)
	{
		Controller->LogSearchTransitionDebug(TEXT("SearchExhausted"), TEXT("수색 횟수 소진: 선택 태스크 Failed"));
		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 종료 - %d개 지점을 훑었다. 순찰로 복귀."),
			*GetNameSafe(GuardPawn), SearchSweepCount);
		return false;
	}

	// 조사 지점 주변에서 실제로 도달 가능한 지점만 고른다.
	// 무작위 오프셋을 그냥 더하면 벽 너머나 NavMesh 밖이 나와 Move To 가 실패한다.
	FVector SweepLocation;

	if (FindReachableSearchLocation(SearchAnchor, SweepLocation, false))
	{
		BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, SweepLocation);

		UE_LOG(LogGuardAI, Log, TEXT("[%s] 수색 %d/%d - %s"),
			*GetNameSafe(GuardPawn), CurrentSearchStep, SearchSweepCount,
			*SweepLocation.ToCompactString());
		return true;
	}

	Controller->LogSearchTransitionDebug(TEXT("SearchPointFailed"), TEXT("주변 도달 가능한 수색 지점 추출 실패"));
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
