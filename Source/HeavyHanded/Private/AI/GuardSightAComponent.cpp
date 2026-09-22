// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardSightAComponent.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"

#include "AI/GuardTypes.h"


#include "DrawDebugHelpers.h"
#include "AI/GuardAIController.h"
#include "AI/GuardHearingAComponent.h"
#include "Character/GuardCharacter.h"
#include "Components/CapsuleComponent.h"


#include "GameplayTagContainer.h"
#include "ProceduralMeshComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"


// Sets default values for this component's properties
UGuardSightAComponent::UGuardSightAComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Sight/Hearing 감지 설정은 생성자에서 기본값만 잡는다.
	// 시야각·거리 등 세부 파라미터는 OnPossess -> ApplyGuardStats() 가 DT_GuardStats 에서
	// GuardType 에 맞는 행을 찾아 덮어쓴다. 멤버(UPROPERTY)로 들고 있어야 디테일 패널에도 뜬다.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	//삭제
	//UE_LOG(LogTemp, Warning, TEXT("[SightConfig CONSTRUCTOR] Component=%p | SightConfig=%p | Name=%s"), this, SightConfig.Get(), *GetNameSafe(SightConfig));

	// 플레이어는 IGenericTeamAgentInterface를 구현하지 않아 FGenericTeamId::NoTeam(255)로
	// 남는다. 경비 입장에서 그런 상대는 "중립"으로 판정되므로 bDetectNeutrals를 켜야
	// 플레이어를 감지한다. 경비끼리는 위에서 같은 팀으로 묶어 "우호"로 판정되는데,
	// bDetectFriendlies는 꺼서 서로를 감지 대상에서 제외한다 — 켜두면 경비 2명을 배치했을 때
	// 서로를 시야로 잡고 쫓아다니며 교착 상태에 빠진다.
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;


}

//// pawn 빙의시 초기화중
//void UGuardSightAComponent::InitializeSightPerception(UAIPerceptionComponent* InPerceptionComp)
//{
//
//}


void UGuardSightAComponent::Initialize(AGuardCharacter* InGuardCharacter, UAIPerceptionComponent* InPerceptionComp)
{
	if (!IsValid(InGuardCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardSightAComponent Initialize failed: GuardCharacter is invalid."));
		return;
	}

	if (!IsValid(InPerceptionComp))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardSightAComponent Initialize failed: PerceptionComp is invalid."));
		return;
	}

	if (!IsValid(SightConfig))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardSightAComponent Initialize failed: SightConfig is invalid."));
		return;
	}

	GuardAIController = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(GuardAIController))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardSightAComponent Initialize failed: GuardAIController is invalid."));
		return;
	}

	GuardCharacter = InGuardCharacter;
	PerceptionComp = InPerceptionComp;

	SightDebugMesh = GuardCharacter->GetSightDebugMesh();
	if (!IsValid(SightDebugMesh))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardSightAComponent Initialize failed: SightDebugMesh is invalid."));
		return;
	}

	GuardCapsule = GuardCharacter->GetCapsuleComponent();
	if (!IsValid(GuardCapsule))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardSightAComponent Initialize failed: GuardCapsule is invalid."));
		return;
	}

	PerceptionComp->ConfigureSense(*SightConfig);

	SetSightEnabled(GuardCharacter->IsSightEnabled());
	SetSightDebugEnabled(GuardCharacter->IsDrawSightDebugEnabled());

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


void UGuardSightAComponent::SetSightConfig (float InSightRadius, float InLoseSightRadius,
	float InPeripheralVisionAngle, float InVerticalVisionAngle, float InBinocularVisionAngle)
{
	if (!SightConfig)
	{
		// SightConfig 존재하지 않음 로그
		return;
	}

	SightConfig->SightRadius = InSightRadius;
	SightConfig->LoseSightRadius = InLoseSightRadius;


	SightConfig->PeripheralVisionAngleDegrees = InPeripheralVisionAngle * 0.5f;
	VerticalVisionAngleDegrees = InVerticalVisionAngle;

	// 전체 시야 안에서 중앙 양안 시야에 해당하는 각도를 저장한다.
	// 이후 인지 게이지 상승 속도를 계산할 때 사용한다.
	BinocularVisionAngleDegrees = InBinocularVisionAngle * 0.5f;

}

bool UGuardSightAComponent::IsWithinBinocularVisionAngle(AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	// 대상과 경비 사이의 방향에서 높이 차이를 제거한다.
	// 양안 시야는 수평 시야 기준으로 판단한다.
	FVector ToTarget = TargetActor->GetActorLocation() - OwnerActor->GetActorLocation();
	ToTarget.Z = 0.f;

	if (ToTarget.IsNearlyZero())
	{
		return true;
	}

	ToTarget.Normalize();

	// 경비가 바라보는 방향도 수평 방향만 사용한다.
	FVector Forward = OwnerActor->GetActorForwardVector();
	Forward.Z = 0.f;

	if (Forward.IsNearlyZero())
	{
		return true;
	}

	Forward.Normalize();

	// 정면과 대상 사이의 수평 각도를 계산한다.
	const float Dot = FMath::Clamp(FVector::DotProduct(Forward, ToTarget), -1.f, 1.f);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

	// 설정된 양안 시야각은 좌우를 합친 전체 각도이므로 절반을 사용한다.
	const float BinocularHalfAngle = BinocularVisionAngleDegrees * 0.5f;

	return AngleDegrees <= BinocularHalfAngle;
}

float UGuardSightAComponent::GetBinocularVisionRate(AActor* TargetActor) const
{
	if (IsWithinBinocularVisionAngle(TargetActor))
	{
		return BinocularVisionRate;
	}

	return PeripheralVisionRate;
}

void UGuardSightAComponent::SetSightDebugEnabled(bool bInEnabled)
{
	bDrawSightDebug = bInEnabled;
	SightDebugMesh->SetVisibility(bInEnabled);
	SetComponentTickEnabled(bInEnabled);

	//if (!bInEnabled)
	//{
	//	SightDebugMesh->ClearAllMeshSections();
	//}



}


void UGuardSightAComponent::SetSightEnabled(bool isEnable)
{
	//if (!PerceptionComp) return;
	PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), isEnable);
}



void UGuardSightAComponent::OnTargetPerceptionUpdatedSight
		(AActor* Actor, FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp)
{

	const FGameplayTag GuardDisguiseTag = FGameplayTag::RequestGameplayTag(FName("Ability.Mimic.GuardDisguise"));

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);

	if (TargetASC && TargetASC->HasMatchingGameplayTag(GuardDisguiseTag))
	{
		return;
	}


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
			//AGuardAIController* GuardAIController = Cast<AGuardAIController>(GetOwner());
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


#if WITH_EDITOR
	if (bDrawSightDebug)
	{
		DrawSightDebug();
		DrawSightDebugMesh();
		DrawPerceivedActorsDebug();
	}

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

	//const AGuardAIController* GuardAIController = Cast<AGuardAIController>(GetOwner());
	//if (!GuardAIController)
	//{
	//	return false;
	//}
	//
	//const AGuardCharacter* GuardCharacter = Cast<AGuardCharacter>(GuardAIController->GetPawn());
	//if (!GuardCharacter)
	//{
	//	return false;
	//}

	//const UCapsuleComponent* Capsule = GuardCharacter->GetCapsuleComponent();

	//const FVector GuardEyeLocation = GuardCharacter->GetMesh()->GetSocketLocation(TEXT("head"));
	//const FVector TargetHeadLocation = TargetActor->GetMesh()->GetSocketLocation(TEXT("head"));
	//const FVector GuardEyeLocation = GuardCharacter->GetRootComponent()->GetComponentLocation();// +FVector(0.0f, 0.0f, -80.0f); // 80은 임시값
	//const FVector TargetHeadLocation = TargetActor->GetRootComponent()->GetComponentLocation(); // +FVector(0.0f, 0.0f, -80.0f);

	//const FVector GuardEyeLocation = GuardCharacter->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, GuardCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());


	// 테스트용. 추후 사용시 수정 반드시 필요
	const FVector GuardEyeLocation =
		GuardCharacter->GetRootComponent()->GetComponentLocation() + GuardCharacter->GetActorForwardVector() * GuardCapsule->GetScaledCapsuleRadius() + FVector(0.0f, 0.0f, GuardCharacter->GetEyeHeight());
	//const FVector GuardEyeLocation = GuardCharacter->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, GuardCharacter->GetEyeHeight());
	const FVector TargetHeadLocation = TargetActor->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, GuardCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	const FVector Direction = (TargetHeadLocation - GuardEyeLocation).GetSafeNormal();

	const float VerticalAngle = FMath::RadiansToDegrees(FMath::Asin(Direction.Z));

	return FMath::Abs(VerticalAngle) <= VerticalVisionAngleDegrees;

}


void UGuardSightAComponent::DrawSightDebug() const
{
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GuardCharacter->GetRootComponent()->GetComponentLocation() + GuardCharacter->GetActorForwardVector() * GuardCapsule->GetScaledCapsuleRadius() + FVector(0.0f, 0.0f, GuardCharacter->GetEyeHeight());
	const FVector HorizontalOrigin = GuardCharacter->GetCapsuleComponent()->GetComponentLocation() - FVector(0.0f, 0.0f, GuardCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	const FVector Forward = GuardCharacter->GetActorForwardVector().GetSafeNormal();
	const FVector Up = FVector::UpVector;

	const float SightRadius = SightConfig->SightRadius;
	const float LoseSightRadius = SightConfig->LoseSightRadius;

	const float HorizontalHalfAngle = SightConfig->PeripheralVisionAngleDegrees;
	const float VerticalHalfAngle = VerticalVisionAngleDegrees;

	if (SightRadius <= 0.0f || LoseSightRadius <= SightRadius || HorizontalHalfAngle <= 0.0f || VerticalHalfAngle <= 0.0f)
	{
		return;
	}

	constexpr float CharacterDebugOffset = 50.0f;

	const int32 ArcSegments = 24;
	const float VerticalAngleRadians = FMath::DegreesToRadians(VerticalHalfAngle);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSightDebug), true, GuardCharacter);

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

		const FVector TraceEnd = HorizontalOrigin + Direction * LoseSightRadius;

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, HorizontalOrigin, TraceEnd, ECC_Visibility, Params);

		float VisibleDistance = bBlocked ? FVector::Distance(HorizontalOrigin, Hit.Location) : LoseSightRadius;

		const bool bHitCharacter = bBlocked && Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>();

		if (bHitCharacter)
		{
			VisibleDistance = FMath::Min(VisibleDistance + CharacterDebugOffset, LoseSightRadius);
		}

		const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);
		const float YellowDistance = FMath::Min(LoseSightRadius, VisibleDistance);

		const FVector GreenPoint = HorizontalOrigin + Direction * GreenDistance;
		const FVector YellowPoint = HorizontalOrigin + Direction * YellowDistance;

		if (bHasPreviousGreenPoint)
		{
			DrawDebugLine(World, PreviousGreenPoint, GreenPoint, FColor::Green, false, 0.0f, 0, 4.0f);
		}

		if (VisibleDistance > SightRadius && bHasPreviousYellowPoint)
		{
			DrawDebugLine(World, PreviousYellowPoint, YellowPoint, FColor::Yellow, false, 0.0f, 0, 4.0f);
		}

		if (bBlocked)
		{
			const FVector RedStart = bHitCharacter ? Hit.Location + Direction * CharacterDebugOffset : Hit.Location;

			DrawDebugLine(World, RedStart, TraceEnd, FColor::Red, false, 0.0f, 0, 2.0f);
		}

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

		float UpVisibleDistance = bUpBlocked ? FVector::Distance(Origin, UpHit.Location) : LoseSightRadius;
		float DownVisibleDistance = bDownBlocked ? FVector::Distance(Origin, DownHit.Location) : LoseSightRadius;

		const bool bUpHitCharacter = bUpBlocked && UpHit.GetActor() && UpHit.GetActor()->IsA<ACharacter>();
		const bool bDownHitCharacter = bDownBlocked && DownHit.GetActor() && DownHit.GetActor()->IsA<ACharacter>();

		if (bUpHitCharacter)
		{
			UpVisibleDistance = FMath::Min(UpVisibleDistance + CharacterDebugOffset, LoseSightRadius);
		}

		if (bDownHitCharacter)
		{
			DownVisibleDistance = FMath::Min(DownVisibleDistance + CharacterDebugOffset, LoseSightRadius);
		}

		const float UpGreenDistance = FMath::Min(SightRadius, UpVisibleDistance);
		const float DownGreenDistance = FMath::Min(SightRadius, DownVisibleDistance);

		DrawDebugLine(World, Origin, Origin + UpDirection * UpGreenDistance, FColor::Green, false, 0.0f, 0, 4.0f);
		DrawDebugLine(World, Origin, Origin + DownDirection * DownGreenDistance, FColor::Green, false, 0.0f, 0, 4.0f);

		if (UpVisibleDistance > SightRadius)
		{
			DrawDebugLine(World, Origin + UpDirection * SightRadius, Origin + UpDirection * UpVisibleDistance, FColor::Yellow, false, 0.0f, 0, 4.0f);
		}

		if (DownVisibleDistance > SightRadius)
		{
			DrawDebugLine(World, Origin + DownDirection * SightRadius, Origin + DownDirection * DownVisibleDistance, FColor::Yellow, false, 0.0f, 0, 4.0f);
		}

		if (bUpBlocked)
		{
			const FVector RedStart = bUpHitCharacter ? UpHit.Location + UpDirection * CharacterDebugOffset : UpHit.Location;

			DrawDebugLine(World, RedStart, UpTraceEnd, FColor::Red, false, 0.0f, 0, 2.0f);
		}

		if (bDownBlocked)
		{
			const FVector RedStart = bDownHitCharacter ? DownHit.Location + DownDirection * CharacterDebugOffset : DownHit.Location;

			DrawDebugLine(World, RedStart, DownTraceEnd, FColor::Red, false, 0.0f, 0, 2.0f);
		}
	}

	// ========================================================
	// 수평 좌우 경계
	// ========================================================

	{
		const FVector LeftDirection = Forward.RotateAngleAxis(-HorizontalHalfAngle, Up);
		const FVector RightDirection = Forward.RotateAngleAxis(HorizontalHalfAngle, Up);

		// 왼쪽
		{
			const FVector TraceEnd = HorizontalOrigin + LeftDirection * LoseSightRadius;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, HorizontalOrigin, TraceEnd, ECC_Visibility, Params);

			float VisibleDistance = bBlocked ? FVector::Distance(HorizontalOrigin, Hit.Location) : LoseSightRadius;

			const bool bHitCharacter = bBlocked && Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>();

			if (bHitCharacter)
			{
				VisibleDistance = FMath::Min(VisibleDistance + CharacterDebugOffset, LoseSightRadius);
			}

			const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);

			DrawDebugLine(World, HorizontalOrigin, HorizontalOrigin + LeftDirection * GreenDistance, FColor::Green, false, 0.0f, 0, 4.0f);

			if (VisibleDistance > SightRadius)
			{
				DrawDebugLine(World, HorizontalOrigin + LeftDirection * SightRadius, HorizontalOrigin + LeftDirection * VisibleDistance, FColor::Yellow, false, 0.0f, 0, 4.0f);
			}

			if (bBlocked)
			{
				const FVector RedStart = bHitCharacter ? Hit.Location + LeftDirection * CharacterDebugOffset : Hit.Location;

				DrawDebugLine(World, RedStart, TraceEnd, FColor::Red, false, 0.0f, 0, 2.0f);
			}
		}

		// 오른쪽
		{
			const FVector TraceEnd = HorizontalOrigin + RightDirection * LoseSightRadius;

			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, HorizontalOrigin, TraceEnd, ECC_Visibility, Params);

			float VisibleDistance = bBlocked ? FVector::Distance(HorizontalOrigin, Hit.Location) : LoseSightRadius;

			const bool bHitCharacter = bBlocked && Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>();

			if (bHitCharacter)
			{
				VisibleDistance = FMath::Min(VisibleDistance + CharacterDebugOffset, LoseSightRadius);
			}

			const float GreenDistance = FMath::Min(SightRadius, VisibleDistance);

			DrawDebugLine(World, HorizontalOrigin, HorizontalOrigin + RightDirection * GreenDistance, FColor::Green, false, 0.0f, 0, 4.0f);

			if (VisibleDistance > SightRadius)
			{
				DrawDebugLine(World, HorizontalOrigin + RightDirection * SightRadius, HorizontalOrigin + RightDirection * VisibleDistance, FColor::Yellow, false, 0.0f, 0, 4.0f);
			}

			if (bBlocked)
			{
				const FVector RedStart = bHitCharacter ? Hit.Location + RightDirection * CharacterDebugOffset : Hit.Location;

				DrawDebugLine(World, RedStart, TraceEnd, FColor::Red, false, 0.0f, 0, 2.0f);
			}
		}

		// ========================================================
		// 양안 시야 경계
		// ========================================================

		if (BinocularVisionAngleDegrees > 0.0f)
		{
			const float BinocularHalfAngle = BinocularVisionAngleDegrees * 0.5f;

			const FVector BinocularLeftDirection = Forward.RotateAngleAxis(-BinocularHalfAngle, Up);
			const FVector BinocularRightDirection = Forward.RotateAngleAxis(BinocularHalfAngle, Up);

			DrawDebugLine(World, HorizontalOrigin, HorizontalOrigin + BinocularLeftDirection * SightRadius, FColor::Cyan, false, 0.0f, 0, 2.0f);
			DrawDebugLine(World, HorizontalOrigin, HorizontalOrigin + BinocularRightDirection * SightRadius, FColor::Cyan, false, 0.0f, 0, 2.0f);
		}


	}
}


void UGuardSightAComponent::DrawSightDebugMesh()
{
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 실제 시야 거리와 수평 시야각을 가져온다.
	// VerticalVisionAngleDegrees는 여기서는 사용하지 않는다.
	const float SightRadius = SightConfig->SightRadius;
	const float HalfAngle = SightConfig->PeripheralVisionAngleDegrees;

	if (SightRadius <= 0.0f || HalfAngle <= 0.0f)
	{
		SightDebugMesh->ClearAllMeshSections();
		return;
	}

	// 시야 부채꼴을 몇 개의 조각으로 나눌지 결정한다.
	// 숫자가 높을수록 곡선이 부드러워지지만 Mesh 계산량이 증가한다.
	const int32 ArcSegments = 48;

	// 캡슐의 절반 높이를 구한다.
	const float CapsuleHalfHeight = GuardCapsule->GetScaledCapsuleHalfHeight();

	// Mesh의 중심점.
	// 캐릭터의 캡슐 바닥에서 2cm 위에 배치한다.
	const FVector LocalOrigin(0.0f, 0.0f, -CapsuleHalfHeight + 2.0f);

	// Procedural Mesh가 실제로 배치된 Transform을 사용한다.
	// Trace는 World 좌표를 사용하고 Mesh Vertex는 Local 좌표를 사용해야 하므로
	// 두 좌표계를 변환할 때 이 Transform을 기준으로 사용한다.
	const FTransform MeshTransform = SightDebugMesh->GetComponentTransform();

	// Mesh 중심점을 World 좌표로 변환한다.
	// Line Trace의 시작점은 World 좌표여야 한다.
	const FVector WorldOrigin = MeshTransform.TransformPosition(LocalOrigin);

	// Guard 자신은 시야 Trace에 맞지 않으므로 무시한다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSightDebug), true, GuardCharacter);

	// Procedural Mesh에 사용할 Vertex와 Triangle 데이터를 준비한다.
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	// 중심점 1개 + 부채꼴 외곽점들을 저장한다.
	Vertices.Reserve(ArcSegments + 2);

	// 각 구간마다 Triangle 하나씩 생성한다.
	Triangles.Reserve(ArcSegments * 3);

	// 부채꼴의 중심점을 첫 번째 Vertex로 등록한다.
	Vertices.Add(LocalOrigin);

	// 왼쪽 시야 끝에서 오른쪽 시야 끝까지 한 칸씩 검사한다.
	for (int32 Index = 0; Index <= ArcSegments; ++Index)
	{
		// 현재 지점이 전체 시야각에서 몇 % 위치인지 계산한다.
		const float Alpha = static_cast<float>(Index) / static_cast<float>(ArcSegments);

		// -HalfAngle ~ +HalfAngle 사이의 현재 각도를 계산한다.
		const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, Alpha);

		// Mesh 기준의 로컬 방향을 만든다.
		// 캐릭터 정면을 기준으로 좌우로 회전시키므로 수평 시야만 만들어진다.
		const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, FVector::UpVector);

		// Trace를 하기 위해 로컬 방향을 World 방향으로 변환한다.
		const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();

		// 현재 방향으로 SightRadius만큼 Line Trace한다.
		const FVector TraceEnd = WorldOrigin + WorldDirection * SightRadius;

		FHitResult Hit;

		// 장애물이 있는지 검사한다.
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility, Params);

		// 기본적으로 장애물이 없으면 SightRadius까지 Mesh를 만든다.
		float VisibleDistance = SightRadius;

		if (bBlocked)
		{
			// 장애물이 있다면 장애물이 맞은 지점까지만 Mesh를 만든다.
			VisibleDistance = FMath::Clamp(FVector::Distance(WorldOrigin, Hit.Location), 0.0f, SightRadius);

			// 캐릭터를 맞춘 경우에는 Mesh가 캐릭터 중심에서 너무 빨리 끊겨 보이지 않도록
			// 50cm를 추가한다.
			if (Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>())
			{
				VisibleDistance = FMath::Min(VisibleDistance + 50.0f, SightRadius);
			}
		}

		// Mesh Vertex는 Local 좌표로 넣어야 한다.
		// 따라서 LocalDirection을 사용해서 실제 보이는 거리까지만 Vertex를 만든다.
		Vertices.Add(LocalOrigin + LocalDirection * VisibleDistance);
	}

	// 중심점과 인접한 외곽점 2개를 연결해서 삼각형을 만든다.
	// 모든 삼각형이 위쪽을 향하도록 Vertex 순서를 반대로 구성한다.
	for (int32 Index = 0; Index < ArcSegments; ++Index)
	{
		Triangles.Add(0);
		Triangles.Add(Index + 2);
		Triangles.Add(Index + 1);
	}

	// 모든 Vertex가 수평면을 향하도록 Normal을 설정한다.
	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		Normals.Add(FVector::UpVector);
		UV0.Add(FVector2D::ZeroVector);
		VertexColors.Add(FLinearColor::White);
	}

	// 이전 프레임에 만들어진 Mesh를 제거한다.
	SightDebugMesh->ClearAllMeshSections();

	// 계산한 Vertex와 Triangle로 새로운 시야 Mesh를 만든다.
	SightDebugMesh->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UV0,
		VertexColors,
		Tangents,
		false
	);

	// 디버그 Mesh를 화면에 표시한다.
	SightDebugMesh->SetVisibility(true);
	SightDebugMesh->SetHiddenInGame(false);

	// 시야 디버그 Mesh가 충돌에 영향을 주지 않도록 한다.
	SightDebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 지정된 디버그용 Material을 적용한다.
	if (SightDebugMaterial)
	{
		SightDebugMesh->SetMaterial(0, SightDebugMaterial);
	}
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

		DrawDebugLine(World, Origin, TargetLocation, FColor::Yellow, false, 0.0f, 0, 6.0f);
		DrawDebugSphere(World, TargetLocation, 25.0f, 12, FColor::Yellow, false, 0.0f);
	}
}


