// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardSightAComponent.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"

#include "AI/GuardTypes.h"


#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "AI/GuardAIController.h"
#include "AI/GuardHearingAComponent.h"
#include "Character/GuardCharacter.h"


// Sets default values for this component's properties
UGuardSightAComponent::UGuardSightAComponent()
{

	// Sight/Hearing 감지 설정은 생성자에서 기본값만 잡는다.
	// 시야각·거리 등 세부 파라미터는 OnPossess -> ApplyGuardStats() 가 DT_GuardStats 에서
	// GuardType 에 맞는 행을 찾아 덮어쓴다. 멤버(UPROPERTY)로 들고 있어야 디테일 패널에도 뜬다.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	UE_LOG(LogTemp, Warning, TEXT("[SightConfig CONSTRUCTOR] Component=%p | SightConfig=%p | Name=%s"), this, SightConfig.Get(), *GetNameSafe(SightConfig));

	// 플레이어는 IGenericTeamAgentInterface를 구현하지 않아 FGenericTeamId::NoTeam(255)로
	// 남는다. 경비 입장에서 그런 상대는 "중립"으로 판정되므로 bDetectNeutrals를 켜야
	// 플레이어를 감지한다. 경비끼리는 위에서 같은 팀으로 묶어 "우호"로 판정되는데,
	// bDetectFriendlies는 꺼서 서로를 감지 대상에서 제외한다 — 켜두면 경비 2명을 배치했을 때
	// 서로를 시야로 잡고 쫓아다니며 교착 상태에 빠진다.
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;


}

void UGuardSightAComponent::InitializeSightPerception(UAIPerceptionComponent* InPerceptionComp)
{
	PerceptionComp = InPerceptionComp;

	if (IsValid(PerceptionComp) && IsValid(SightConfig))
	{
		PerceptionComp->ConfigureSense(*SightConfig);
	}

}



// Called when the game starts
void UGuardSightAComponent::BeginPlay()
{
	Super::BeginPlay();

}


void UGuardSightAComponent::OnRegister()
{
	Super::OnRegister();

}


void UGuardSightAComponent::SetSightConfig
(float InSightRadius, float InLoseSightRadius, float InPeripheralVisionAngle, float InVerticalVisionAngle)
{
	if (!SightConfig)
	{
		// SightConfig 존재하지 않음 로그
		return;
	}

	SightConfig->SightRadius = InSightRadius;
	SightConfig->LoseSightRadius = InLoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = InPeripheralVisionAngle*0.5f;
	VerticalVisionAngleDegrees = InVerticalVisionAngle;

}


void UGuardSightAComponent::SetSightEnabled(bool isEnable)
{
	//if (!PerceptionComp) return;

	PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), isEnable);
	PrimaryComponentTick.bCanEverTick = isEnable;
}



void UGuardSightAComponent::OnTargetPerceptionUpdatedSight
		(AActor* Actor, FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp)
{

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		// 시야 획득/상실이 초당 여러 번 뒤집히면 추격 브랜치가 그만큼 abort/restart 된다.
		// 눈으로 세기 어려우므로 상실이 실제로 몇 초 지속됐는지를 같이 찍는다.
		// 1초 미만이 반복되면 깜빡임, 수 초 단위면 정상적으로 놓친 것이다.
		const float NowSeconds = GetWorld()->GetTimeSeconds();


		if (Stimulus.WasSuccessfullySensed())
		{
			if (SightLostAtTime >= 0.f)
			{
				// GetPawn = 상위 가져오기
				//UE_LOG(LogGuardAI, Log, TEXT("[%s] 시야 획득: %s (직전 상실이 %.2f초 지속)"),
				//	*GetNameSafe(GetPawn()), *GetNameSafe(Actor), NowSeconds - SightLostAtTime);
			}
			else
			{
				//UE_LOG(LogGuardAI, Log, TEXT("[%s] 시야 획득: %s (최초)"),
				//	*GetNameSafe(GetPawn()), *GetNameSafe(Actor));
			}

			SightLostAtTime = -1.f;
		}
		else
		{
			SightLostAtTime = NowSeconds;

			//UE_LOG(LogGuardAI, Log, TEXT("[%s] 시야 상실: %s"),
			//	*GetNameSafe(GetPawn()), *GetNameSafe(Actor));
		}

		// 브로드캐스트는 false->true 전환 1회로 제한한다 - 덮어쓰기 전에 이전 값을 봐둔다.
		const bool bWasSeeing = BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget);

		BlackboardComp->SetValueAsBool(GuardAIKeys::CanSeeTarget, Stimulus.WasSuccessfullySensed());

		if (Stimulus.WasSuccessfullySensed())
		{

			// Sight에서 플레이어를 발견했을 때 Hearing에서 걸어둔 FocalPoint를 해제
			AGuardAIController* GuardAIController = Cast<AGuardAIController>(GetOwner());
			if (GuardAIController)
			{
				GuardAIController->ClearFocus(EAIFocusPriority::Gameplay);


				if (GuardAIController->GuardHearingComp)
				{
					GuardAIController->GuardHearingComp->ClearHearingDebug();
				}
			}

			// 경비의 실제 눈높이에서 플레이어 머리까지의 수직 시야각을 검사
			if (!IsWithinVerticalVisionAngle(Actor))
			{
				BlackboardComp->SetValueAsBool(GuardAIKeys::CanSeeTarget, false);
			}

			else
			{
				if (!bWasSeeing)
				{
					// 필요한지 확인 한번 더하고 주석 풀 것
					/// OnPlayerSpotted.Broadcast(Actor);
				}

				BlackboardComp->SetValueAsObject(GuardAIKeys::TargetActor, Actor);
				BlackboardComp->SetValueAsVector(GuardAIKeys::LastKnownLocation, Stimulus.StimulusLocation);

				// 시야 경계에서 감지가 프레임 단위로 깜빡여도 추격을 바로 이탈하지 않도록,
				// 실제로 "본" 순간마다 시각을 갱신한다. BT 추격 브랜치의
				// Check Search Timeout(TimeKeyName=LastSeenTime, TimeoutSeconds=1.5)이 이 값을 읽는다.
				// 이 write 가 없으면 OnPossess 의 초기값(-100000)이 그대로 남아
				// 추격 조건이 영구히 거짓이 된다.
				BlackboardComp->SetValueAsFloat(GuardAIKeys::LastSeenTime, GetWorld()->GetTimeSeconds());
			}
		}
		// 시야를 잃었다고 해서 여기서 SearchStartTime 을 쓰지 않는다.
		//
		// 쓰면 스쳐 지나가듯 한 번 보이기만 해도 조사가 켜진다. Guard.ini 는
		// Guard.State.Investigate 를 "인지 게이지가 가득 차" 진입하는 상태로 정의한다.
		// 그 조건은 BTService_UpdateDetectionGauge 가 게이지 100 인 동안 매 틱
		// SearchStartTime 을 밀어주는 것으로 이미 만족된다 - 시야를 잃는 순간
		// 그 값이 얼어붙어 자연스럽게 "수색 시작 시각"이 된다.
	}

}











// Called every frame
void UGuardSightAComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...

	//const AGuardAIController* GuardController = Cast<AGuardAIController>(GetOwner());
	//if (GuardController && GuardController->GetPawn())
	//{
	//	DrawDebugSphere(GetWorld(), GuardController->GetPawn()->GetActorLocation(), 100.0f, 16, FColor::Yellow, false, 0.1f);
	//}



#if WITH_EDITOR
	DrawSightDebug();
	DrawPerceivedActorsDebug();
#endif

	// ...


}



//경비의 눈 위치 → 플레이어 머리 위치를 기준으로 계산
bool UGuardSightAComponent::IsWithinVerticalVisionAngle(AActor* TargetActor) const
{

	if (!IsValid(TargetActor))
	{
		return false;
	}

	const AGuardAIController* GuardAIController = Cast<AGuardAIController>(GetOwner());
	if (!GuardAIController)
	{
		return false;
	}

	const AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(GuardAIController->GetPawn());
	if (!GuardCharacter)
	{
		return false;
	}

	//const FVector GuardEyeLocation = GuardCharacter->GetMesh()->GetSocketLocation(TEXT("head"));
	//const FVector TargetHeadLocation = TargetActor->GetMesh()->GetSocketLocation(TEXT("head"));
	const FVector GuardEyeLocation = GuardCharacter->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, 80.0f); // 80은 임시값
	const FVector TargetHeadLocation = TargetActor->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, 80.0f);
	const FVector Direction = (TargetHeadLocation - GuardEyeLocation).GetSafeNormal();

	const float VerticalAngle = FMath::RadiansToDegrees(FMath::Asin(Direction.Z));

	return FMath::Abs(VerticalAngle) <= VerticalVisionAngleDegrees;

}


void UGuardSightAComponent::DrawSightDebug() const
{
	if (!SightConfig)
	{
		return;
	}

	const AGuardAIController* GuardController = Cast<AGuardAIController>(GetOwner());
	if (!GuardController)
	{
		return;
	}

	const APawn* GuardPawn = GuardController->GetPawn();
	if (!GuardPawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(GuardPawn);
	if (!GuardCharacter || !GuardCharacter->GetMesh())
	{
		return;
	}

	const FVector Origin = GuardCharacter->GetMesh()->GetSocketLocation(TEXT("head"));
	const FVector Forward = GuardPawn->GetActorForwardVector().GetSafeNormal();
	const FVector Up = FVector::UpVector;

	const float SightRadius = SightConfig->SightRadius;
	const float LoseSightRadius = SightConfig->LoseSightRadius;

	const float HorizontalHalfAngle = SightConfig->PeripheralVisionAngleDegrees;
	const float VerticalHalfAngle = VerticalVisionAngleDegrees;

	if (SightRadius <= 0.0f || LoseSightRadius <= SightRadius || HorizontalHalfAngle <= 0.0f || VerticalHalfAngle <= 0.0f)
	{
		return;
	}

	const int32 ArcSegments = 24;
	const float VerticalAngleRadians = FMath::DegreesToRadians(VerticalHalfAngle);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSightDebug), true, GuardPawn);

	// ========================================================
	// 수평 시야 외곽
	// ========================================================

	FVector PreviousGreenPoint = FVector::ZeroVector;
	FVector PreviousYellowPoint = FVector::ZeroVector;

	bool bHasPreviousGreenPoint = false;
	bool bHasPreviousYellowPoint = false;

	for (int32 Index = 0; Index <= ArcSegments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / ArcSegments;
		const float Angle = FMath::Lerp(-HorizontalHalfAngle, HorizontalHalfAngle, Alpha);
		const FVector Direction = Forward.RotateAngleAxis(Angle, Up);

		const FVector TraceEnd = Origin + Direction * LoseSightRadius;

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, TraceEnd, ECC_Visibility, Params);

		const float VisibleDistance = bBlocked ? FVector::Distance(Origin, Hit.Location) : LoseSightRadius;

		const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);
		const float YellowDistance = FMath::Min(LoseSightRadius, VisibleDistance);

		const FVector GreenPoint = Origin + Direction * GreenDistance;
		const FVector YellowPoint = Origin + Direction * YellowDistance;

		if (bHasPreviousGreenPoint)
		{
			DrawDebugLine(World, PreviousGreenPoint, GreenPoint, FColor::Green, false, 0.1f, 0, 4.0f);
		}

		if (VisibleDistance > SightRadius && bHasPreviousYellowPoint)
		{
			DrawDebugLine(World, PreviousYellowPoint, YellowPoint, FColor::Yellow, false, 0.1f, 0, 4.0f);
		}

		if (bBlocked)
		{
			DrawDebugLine(World, Hit.Location, TraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
		}

		//if (bBlocked && (Index == 0 || Index == ArcSegments))
		//{
		//	DrawDebugLine(World, Hit.Location, TraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
		//}

		PreviousGreenPoint = GreenPoint;
		bHasPreviousGreenPoint = true;

		if (VisibleDistance > SightRadius)
		{
			PreviousYellowPoint = YellowPoint;
			bHasPreviousYellowPoint = true;
		}
		else
		{
			bHasPreviousYellowPoint = false;
		}
	}

	// ========================================================
	// 수직 시야 경계
	// ========================================================

	{
		const FVector HorizontalForward = Forward.GetSafeNormal2D();

		const FVector UpDirection = (HorizontalForward * FMath::Cos(VerticalAngleRadians) + Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();
		const FVector DownDirection = (HorizontalForward * FMath::Cos(VerticalAngleRadians) - Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();

		const FVector UpTraceEnd = Origin + UpDirection * LoseSightRadius;
		const FVector DownTraceEnd = Origin + DownDirection * LoseSightRadius;

		FHitResult UpHit;
		FHitResult DownHit;

		const bool bUpBlocked = World->LineTraceSingleByChannel(UpHit, Origin, UpTraceEnd, ECC_Visibility, Params);
		const bool bDownBlocked = World->LineTraceSingleByChannel(DownHit, Origin, DownTraceEnd, ECC_Visibility, Params);

		const float UpVisibleDistance = bUpBlocked ? FVector::Distance(Origin, UpHit.Location) : LoseSightRadius;
		const float DownVisibleDistance = bDownBlocked ? FVector::Distance(Origin, DownHit.Location) : LoseSightRadius;

		const float UpGreenDistance = FMath::Min(SightRadius, UpVisibleDistance);
		const float DownGreenDistance = FMath::Min(SightRadius, DownVisibleDistance);

		DrawDebugLine(World, Origin, Origin + UpDirection * UpGreenDistance, FColor::Emerald, false, 0.1f, 0, 4.0f);
		DrawDebugLine(World, Origin, Origin + DownDirection * DownGreenDistance, FColor::Emerald, false, 0.1f, 0, 4.0f);

		if (UpVisibleDistance > SightRadius)
		{
			DrawDebugLine(World, Origin + UpDirection * SightRadius, Origin + UpDirection * UpVisibleDistance, FColor::Yellow, false, 0.1f, 0, 4.0f);
		}

		if (DownVisibleDistance > SightRadius)
		{
			DrawDebugLine(World, Origin + DownDirection * SightRadius, Origin + DownDirection * DownVisibleDistance, FColor::Yellow, false, 0.1f, 0, 4.0f);
		}

		if (bUpBlocked)
		{
			DrawDebugLine(World, UpHit.Location, UpTraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
		}

		if (bDownBlocked)
		{
			DrawDebugLine(World, DownHit.Location, DownTraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
		}
	}

	/*
	// ========================================================
	// 수직 시야 외곽
	// ========================================================

	FVector PreviousUpGreenPoint = FVector::ZeroVector;
	FVector PreviousDownGreenPoint = FVector::ZeroVector;
	FVector PreviousUpYellowPoint = FVector::ZeroVector;
	FVector PreviousDownYellowPoint = FVector::ZeroVector;

	bool bHasPreviousUpGreenPoint = false;
	bool bHasPreviousDownGreenPoint = false;
	bool bHasPreviousUpYellowPoint = false;
	bool bHasPreviousDownYellowPoint = false;

	for (int32 Index = 0; Index <= ArcSegments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / ArcSegments;
		const float Angle = FMath::Lerp(-HorizontalHalfAngle, HorizontalHalfAngle, Alpha);

		const FVector HorizontalDirection = Forward.RotateAngleAxis(Angle, Up);

		const FVector UpDirection = (HorizontalDirection * FMath::Cos(VerticalAngleRadians) + Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();
		const FVector DownDirection = (HorizontalDirection * FMath::Cos(VerticalAngleRadians) - Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();

		// 상단
		{
			const FVector TraceEnd = Origin + UpDirection * LoseSightRadius;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, TraceEnd, ECC_Visibility, Params);

			const float VisibleDistance = bBlocked ? FVector::Distance(Origin, Hit.Location) : LoseSightRadius;

			const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);
			const float YellowDistance = FMath::Min(LoseSightRadius, VisibleDistance);

			const FVector GreenPoint = Origin + UpDirection * GreenDistance;
			const FVector YellowPoint = Origin + UpDirection * YellowDistance;

			if (bHasPreviousUpGreenPoint)
			{
				DrawDebugLine(World, PreviousUpGreenPoint, GreenPoint, FColor::Green, false, 0.1f, 0, 2.0f);
			}

			if (VisibleDistance > SightRadius && bHasPreviousUpYellowPoint)
			{
				DrawDebugLine(World, PreviousUpYellowPoint, YellowPoint, FColor::Yellow, false, 0.1f, 0, 2.0f);
			}

			if (bBlocked)
			{
				DrawDebugLine(World, Hit.Location, TraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
			}

			PreviousUpGreenPoint = GreenPoint;
			bHasPreviousUpGreenPoint = true;

			if (VisibleDistance > SightRadius)
			{
				PreviousUpYellowPoint = YellowPoint;
				bHasPreviousUpYellowPoint = true;
			}
			else
			{
				bHasPreviousUpYellowPoint = false;
			}
		}

		// 하단
		{
			const FVector TraceEnd = Origin + DownDirection * LoseSightRadius;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, TraceEnd, ECC_Visibility, Params);

			const float VisibleDistance = bBlocked ? FVector::Distance(Origin, Hit.Location) : LoseSightRadius;

			const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);
			const float YellowDistance = FMath::Min(LoseSightRadius, VisibleDistance);

			const FVector GreenPoint = Origin + DownDirection * GreenDistance;
			const FVector YellowPoint = Origin + DownDirection * YellowDistance;

			if (bHasPreviousDownGreenPoint)
			{
				DrawDebugLine(World, PreviousDownGreenPoint, GreenPoint, FColor::Green, false, 0.1f, 0, 2.0f);
			}

			if (VisibleDistance > SightRadius && bHasPreviousDownYellowPoint)
			{
				DrawDebugLine(World, PreviousDownYellowPoint, YellowPoint, FColor::Yellow, false, 0.1f, 0, 2.0f);
			}

			if (bBlocked)
			{
				DrawDebugLine(World, Hit.Location, TraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
			}

			PreviousDownGreenPoint = GreenPoint;
			bHasPreviousDownGreenPoint = true;

			if (VisibleDistance > SightRadius)
			{
				PreviousDownYellowPoint = YellowPoint;
				bHasPreviousDownYellowPoint = true;
			}
			else
			{
				bHasPreviousDownYellowPoint = false;
			}
		}
	}

	*/

	// ========================================================
	// 수평 좌우 경계
	// ========================================================

	{
		const FVector LeftDirection = Forward.RotateAngleAxis(-HorizontalHalfAngle, Up);
		const FVector RightDirection = Forward.RotateAngleAxis(HorizontalHalfAngle, Up);

		// 왼쪽
		{
			const FVector TraceEnd = Origin + LeftDirection * LoseSightRadius;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, TraceEnd, ECC_Visibility, Params);

			const float VisibleDistance = bBlocked ? FVector::Distance(Origin, Hit.Location) : LoseSightRadius;
			const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);

			DrawDebugLine(World, Origin, Origin + LeftDirection * GreenDistance, FColor::Green, false, 0.1f, 0, 4.0f);

			if (VisibleDistance > SightRadius)
			{
				DrawDebugLine(World, Origin + LeftDirection * SightRadius, Origin + LeftDirection * VisibleDistance, FColor::Yellow, false, 0.1f, 0, 4.0f);
			}

			if (bBlocked)
			{
				DrawDebugLine(World, Hit.Location, TraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
			}
		}

		// 오른쪽
		{
			const FVector TraceEnd = Origin + RightDirection * LoseSightRadius;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, TraceEnd, ECC_Visibility, Params);

			const float VisibleDistance = bBlocked ? FVector::Distance(Origin, Hit.Location) : LoseSightRadius;
			const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);

			DrawDebugLine(World, Origin, Origin + RightDirection * GreenDistance, FColor::Green, false, 0.1f, 0, 4.0f);

			if (VisibleDistance > SightRadius)
			{
				DrawDebugLine(World, Origin + RightDirection * SightRadius, Origin + RightDirection * VisibleDistance, FColor::Yellow, false, 0.1f, 0, 4.0f);
			}

			if (bBlocked)
			{
				DrawDebugLine(World, Hit.Location, TraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
			}
		}
	}


	/*
	if (!SightConfig)
	{
		return;
	}

	const AGuardAIController* GuardController = Cast<AGuardAIController>(GetOwner());
	if (!GuardController)
	{
		return;
	}

	const APawn* GuardPawn = GuardController->GetPawn();
	if (!GuardPawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(GuardController->GetPawn());
	if (!GuardCharacter)
	{
		return;
	}

	
	const FVector Origin = GuardCharacter->GetMesh()->GetSocketLocation(TEXT("head"));
//	const FVector Origin = GuardPawn->GetActorLocation();

	const FVector Forward = GuardPawn->GetActorForwardVector();
	const FVector Up = FVector::UpVector;
	const float Radius = SightConfig->SightRadius;

	const float HorizontalHalfAngle = SightConfig->PeripheralVisionAngleDegrees;
	const float VerticalHalfAngle = VerticalVisionAngleDegrees;

	if (Radius <= 0.0f || HorizontalHalfAngle <= 0.0f)
	{
		return;
	}

	const int32 ArcSegments = 24;
	const int32 DepthSegments = 1;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSightDebug), true, GuardPawn);

	// 외곽 부채꼴
	FVector PreviousPoint = FVector::ZeroVector;
	bool bHasPreviousPoint = false;

	for (int32 Index = 0; Index <= ArcSegments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / ArcSegments;
		const float Angle = FMath::Lerp(-HorizontalHalfAngle, HorizontalHalfAngle, Alpha);
		const FVector Direction = Forward.RotateAngleAxis(Angle, Up);
		const FVector TraceEnd = Origin + Direction * Radius;

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, TraceEnd, ECC_Visibility, Params);

		const FVector VisibleEnd = bBlocked ? Hit.Location : TraceEnd;

		if (bHasPreviousPoint)
		{
			DrawDebugLine(World, PreviousPoint, VisibleEnd, FColor::Green, false, 0.1f, 0, 2.0f);
		}

		if (bBlocked)
		{
			DrawDebugLine(World, Hit.Location, TraceEnd, FColor::Red, false, 0.1f, 0, 2.0f);
		}

		PreviousPoint = VisibleEnd;
		bHasPreviousPoint = true;
	}

	// 좌우 시야 경계
	{
		const FVector LeftDirection = Forward.RotateAngleAxis(-HorizontalHalfAngle, Up);
		const FVector RightDirection = Forward.RotateAngleAxis(HorizontalHalfAngle, Up);

		FHitResult LeftHit;
		FHitResult RightHit;

		const bool bLeftBlocked = World->LineTraceSingleByChannel(LeftHit, Origin, Origin + LeftDirection * Radius, ECC_Visibility, Params);
		const bool bRightBlocked = World->LineTraceSingleByChannel(RightHit, Origin, Origin + RightDirection * Radius, ECC_Visibility, Params);

		const FVector LeftVisibleEnd = bLeftBlocked ? LeftHit.Location : Origin + LeftDirection * Radius;
		const FVector RightVisibleEnd = bRightBlocked ? RightHit.Location : Origin + RightDirection * Radius;

		DrawDebugLine(World, Origin, LeftVisibleEnd, FColor::Green, false, 0.1f, 0, 4.0f);
		DrawDebugLine(World, Origin, RightVisibleEnd, FColor::Green, false, 0.1f, 0, 4.0f);

		if (bLeftBlocked)
		{
			DrawDebugLine(World, LeftHit.Location, Origin + LeftDirection * Radius, FColor::Red, false, 0.1f, 0, 2.0f);
		}

		if (bRightBlocked)
		{
			DrawDebugLine(World, RightHit.Location, Origin + RightDirection * Radius, FColor::Red, false, 0.1f, 0, 2.0f);
		}
	}

	// 상하 수직 시야 경계
	{
		const float VerticalAngleRadians = FMath::DegreesToRadians(VerticalVisionAngleDegrees);
		const FVector HorizontalForward = Forward.GetSafeNormal2D();

		const FVector UpDirection = (HorizontalForward * FMath::Cos(VerticalAngleRadians) + Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();
		const FVector DownDirection = (HorizontalForward * FMath::Cos(VerticalAngleRadians) - Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();

		const FVector UpEnd = Origin + UpDirection * Radius;
		const FVector DownEnd = Origin + DownDirection * Radius;

		DrawDebugLine(World, Origin, UpEnd, FColor::Emerald, false, 0.1f, 0, 4.0f);
		DrawDebugLine(World, Origin, DownEnd, FColor::Emerald, false, 0.1f, 0, 4.0f);
	}


	// 내부 깊이선

	const float SightRadius = SightConfig->SightRadius;
	const float LoseSightRadius = SightConfig->LoseSightRadius;

	for (int32 DepthIndex = 1; DepthIndex <= DepthSegments; ++DepthIndex)
	{
		const float DepthAlpha = static_cast<float>(DepthIndex) / (DepthSegments + 1);
		const float CurrentRadius = SightRadius * DepthAlpha;
		const float VerticalAngleRadians = FMath::DegreesToRadians(VerticalHalfAngle);

		FVector PreviousUpPoint = FVector::ZeroVector;
		FVector PreviousDownPoint = FVector::ZeroVector;
		bool bHasPreviousUpPoint = false;
		bool bHasPreviousDownPoint = false;

		for (int32 Index = 0; Index <= ArcSegments; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / ArcSegments;
			const float Angle = FMath::Lerp(-HorizontalHalfAngle, HorizontalHalfAngle, Alpha);
			const FVector HorizontalDirection = Forward.RotateAngleAxis(Angle, Up);

			const FVector UpDirection = (HorizontalDirection * FMath::Cos(VerticalAngleRadians) + Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();
			const FVector DownDirection = (HorizontalDirection * FMath::Cos(VerticalAngleRadians) - Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();

			const FVector UpTraceEnd = Origin + UpDirection * CurrentRadius;
			const FVector DownTraceEnd = Origin + DownDirection * CurrentRadius;

			FHitResult UpHit;
			FHitResult DownHit;

			const bool bUpBlocked = World->LineTraceSingleByChannel(UpHit, Origin, UpTraceEnd, ECC_Visibility, Params);
			const bool bDownBlocked = World->LineTraceSingleByChannel(DownHit, Origin, DownTraceEnd, ECC_Visibility, Params);

			const FVector UpEndPoint = bUpBlocked ? UpHit.Location : UpTraceEnd;
			const FVector DownEndPoint = bDownBlocked ? DownHit.Location : DownTraceEnd;

			if (bHasPreviousUpPoint)
			{
				DrawDebugLine(World, PreviousUpPoint, UpEndPoint, FColor::Green, false, 0.1f, 0, 0.5f);
			}

			if (bHasPreviousDownPoint)
			{
				DrawDebugLine(World, PreviousDownPoint, DownEndPoint, FColor::Green, false, 0.1f, 0, 0.5f);
			}

			PreviousUpPoint = UpEndPoint;
			PreviousDownPoint = DownEndPoint;
			bHasPreviousUpPoint = true;
			bHasPreviousDownPoint = true;
		}
	}

	const float LoseSightVerticalRadius = LoseSightRadius;
	const float VerticalAngleRadians = FMath::DegreesToRadians(VerticalHalfAngle);

	FVector PreviousLoseUpPoint = FVector::ZeroVector;
	FVector PreviousLoseDownPoint = FVector::ZeroVector;
	bool bHasPreviousLoseUpPoint = false;
	bool bHasPreviousLoseDownPoint = false;

	for (int32 Index = 0; Index <= ArcSegments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / ArcSegments;
		const float Angle = FMath::Lerp(-HorizontalHalfAngle, HorizontalHalfAngle, Alpha);
		const FVector HorizontalDirection = Forward.RotateAngleAxis(Angle, Up);

		const FVector UpDirection = (HorizontalDirection * FMath::Cos(VerticalAngleRadians) + Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();
		const FVector DownDirection = (HorizontalDirection * FMath::Cos(VerticalAngleRadians) - Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();

		const FVector UpPoint = Origin + UpDirection * LoseSightVerticalRadius;
		const FVector DownPoint = Origin + DownDirection * LoseSightVerticalRadius;

		if (bHasPreviousLoseUpPoint)
		{
			DrawDebugLine(World, PreviousLoseUpPoint, UpPoint, FColor::Yellow, false, 0.1f, 0, 1.5f);
		}

		if (bHasPreviousLoseDownPoint)
		{
			DrawDebugLine(World, PreviousLoseDownPoint, DownPoint, FColor::Yellow, false, 0.1f, 0, 1.5f);
		}

		PreviousLoseUpPoint = UpPoint;
		PreviousLoseDownPoint = DownPoint;
		bHasPreviousLoseUpPoint = true;
		bHasPreviousLoseDownPoint = true;
	}




*/

}


// 초록색 = 실제 UAIPerceptionComponent에서 현재 Sight로 인식 중인 Actor
void UGuardSightAComponent::DrawPerceivedActorsDebug() const
{
	if (!PerceptionComp)
	{
		return;
	}

	const AGuardAIController* GuardController = Cast<AGuardAIController>(GetOwner());
	if (!GuardController)
	{
		return;
	}

	const APawn* GuardPawn = GuardController->GetPawn();
	if (!GuardPawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> PerceivedActors;
	PerceptionComp->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	const FVector Origin = GuardPawn->GetActorLocation();

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor)
		{
			continue;
		}

		const FVector TargetLocation = Actor->GetActorLocation();

		DrawDebugLine(World, Origin, TargetLocation, FColor::Yellow, false, 0.1f, 0, 6.0f);
		DrawDebugSphere(World, TargetLocation, 25.0f, 12, FColor::Yellow, false, 0.1f);
	}
}


