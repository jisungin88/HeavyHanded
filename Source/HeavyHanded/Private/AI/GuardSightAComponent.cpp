// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardSightAComponent.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"

#include "AI/GuardTypes.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Shared/NetAuthority.h"


#include "DrawDebugHelpers.h"
#include "AI/GuardAIController.h"
#include "AI/GuardHearingAComponent.h"
#include "Character/GuardCharacter.h"
#include "Character/BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"


#include "ProceduralMeshComponent.h"
#include "TimerManager.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"


// Sets default values for this component's properties
UGuardSightAComponent::UGuardSightAComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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
	if (USkeletalMeshComponent* CharacterMesh = GuardCharacter->GetMesh())
	{
		PrimaryComponentTick.AddPrerequisite(CharacterMesh, CharacterMesh->PrimaryComponentTick);
	}

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
	// 생성 시 확정된 옵션으로 디버그 라인만 관리한다. 스킬 호출로는 바꾸지 않는다.
	bDrawSightDebug = GuardCharacter->IsDrawSightDebugEnabled();
	SetComponentTickEnabled(bDrawSightDebug);
	if (GetOwner()->HasAuthority())
	{
		GuardCharacter->SetReplicatedSightDebugState(
			SightConfig->SightRadius, SightConfig->PeripheralVisionAngleDegrees);
	}
	else
	{
		SightConfig->SightRadius = GuardCharacter->GetReplicatedSightRadius();
		SightConfig->PeripheralVisionAngleDegrees = GuardCharacter->GetReplicatedSightHalfAngle();
	}

	SetSightEnabled(GuardCharacter->IsSightEnabled());
	DrawSightDebugMesh();
	UpdateSightDebugTimer();

}



// Called when the game starts
void UGuardSightAComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateSightDebugTimer();

}

void UGuardSightAComponent::UpdateSightDebugTimer()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogGuardAI, Warning, TEXT("[%s] Sight debug timer not configured: World invalid. bDraw=%s"),
			*GetNameSafe(GuardCharacter), bDrawSightDebug ? TEXT("true") : TEXT("false"));
		return;
	}

	World->GetTimerManager().ClearTimer(SightDebugTimerHandle);
	// 서버는 클라이언트의 스킬 표시를 위해 작은 시야 설정값을 계속 제공한다.
	if (IsValid(GetOwner()) && GetOwner()->HasAuthority())
	{
		const float UpdateInterval = IsValid(GuardCharacter)
			? FMath::Max(GuardCharacter->GetSightDebugUpdateInterval(), 0.05f)
			: 0.25f;
		World->GetTimerManager().SetTimer(
			SightDebugTimerHandle, this, &UGuardSightAComponent::UpdateSightDebug, UpdateInterval, true);
	}

	UE_LOG(LogGuardAI, Warning, TEXT("[%s] Sight debug timer configured. bDraw=%s Active=%s Interval=%.2f WorldBegunPlay=%s"),
		*GetNameSafe(GuardCharacter),
		bDrawSightDebug ? TEXT("true") : TEXT("false"),
		World->GetTimerManager().IsTimerActive(SightDebugTimerHandle) ? TEXT("true") : TEXT("false"),
		IsValid(GuardCharacter) ? GuardCharacter->GetSightDebugUpdateInterval() : 0.25f,
		World->HasBegunPlay() ? TEXT("true") : TEXT("false"));
}

void UGuardSightAComponent::UpdateSightDebug()
{
	//UE_LOG(LogGuardAI, Warning, TEXT("[%s] Sight debug timer fired. Interval=%.2f AIState=%s"),
	//	*GetNameSafe(GuardCharacter),
	//	IsValid(GuardCharacter) ? GuardCharacter->GetSightDebugUpdateInterval() : 0.0f,
	//	IsValid(GuardAIController) ? *StaticEnum<EGuardAIState>()->GetNameStringByValue(static_cast<int64>(GuardAIController->GetAIState())) : TEXT("Invalid"));

	if (IsValid(GuardCharacter))
	{
		if (!GuardCharacter->HasAuthority())
		{
			SightConfig->SightRadius = GuardCharacter->GetReplicatedSightRadius();
			SightConfig->PeripheralVisionAngleDegrees = GuardCharacter->GetReplicatedSightHalfAngle();
		}

		DrawSightDebugMesh();
	}
}

void UGuardSightAComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDrawSightDebug)
	{
		return;
	}

	if (IsValid(GuardCharacter))
	{
		const FTransform EyeTransform = GuardCharacter->GetEyeSocketTransform();
		const FRotator EyeRotation = GuardCharacter->GetSightSocketRotation();
		GuardCharacter->SetReplicatedSightDebugRotation(FRotator(0.0f, EyeRotation.Yaw, 0.0f));

		SightRotationDiagnosticElapsed += DeltaTime;
		if (SightRotationDiagnosticElapsed >= 1.0f)
		{
			SightRotationDiagnosticElapsed = 0.0f;
			/// const USkeletalMeshComponent* CharacterMesh = GuardCharacter->GetMesh();
			/// UE_LOG(LogGuardAI, Warning,
			///	TEXT("[SightRotation] Guard=%s Authority=%d UseSocket=%d SocketExists=%d EyeYaw=%.1f ActorYaw=%.1f MeshYaw=%.1f"),
			///	*GetNameSafe(GuardCharacter),
			///	GuardCharacter->HasAuthority() ? 1 : 0,
			///	GuardCharacter->IsEyeSocketSightEnabled() ? 1 : 0,
			///	IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(TEXT("EyeSocket")) ? 1 : 0,
			///	EyeRotation.Yaw,
			///	GuardCharacter->GetActorRotation().Yaw,
			///	IsValid(SightDebugMesh) ? SightDebugMesh->GetComponentRotation().Yaw : 0.0f);
		}
	}

	// 테스트용 DebugLine과 인지 액터 표시입니다. 테스트가 끝나면 아래 호출을 주석 처리해 Tick 갱신을 끌 수 있습니다.
	DrawSightDebug();
	DrawPerceivedActorsDebug();
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
	if (IsValid(GuardCharacter) && GuardCharacter->HasAuthority())
	{
		GuardCharacter->SetReplicatedSightDebugState(
			SightConfig->SightRadius, SightConfig->PeripheralVisionAngleDegrees);
		DrawSightDebugMesh();
	}

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
	FVector Forward = IsValid(GuardCharacter)
		? GuardCharacter->GetSightSocketRotation().Vector()
		: OwnerActor->GetActorForwardVector();
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
	// 기존 Controller·스킬 호출 경로는 유지하되 시야 메시만 켜고 끈다.
	if (IsValid(GuardCharacter))
	{
		GuardCharacter->SetDrawSightDebugEnabled(bInEnabled);
	}
}


void UGuardSightAComponent::SetSightEnabled(bool bEnabled)
{
	//if (!PerceptionComp) return;
	PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), bEnabled);
}

void UGuardSightAComponent::RefreshSightTarget(UBlackboardComponent* BlackboardComp)
{
	if (!HasServerAuthority(this))
	{
		return;
	}

	if (!IsValid(BlackboardComp) || !IsValid(PerceptionComp) || !IsValid(GuardCharacter) || !IsValid(GuardAIController))
	{
		return;
	}
	// [체포 추가] 담당 경비는 CustodyTarget을 감시한다. 일반 시야 갱신으로 다른 타겟을 잡지 않는다.
	if (GuardAIController->IsInCustody()) return;

	TArray<AActor*> PerceivedActors;
	if (GuardCharacter->IsSightEnabled())
	{
		PerceptionComp->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
	}

	const auto CanDetectActor = [this](AActor* Actor)
	{
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !IsValid(Actor->GetRootComponent()))
		{
			return false;
		}
		if (const ABaseCharacter* Player = Cast<ABaseCharacter>(Actor); IsValid(Player) && Player->IsRestrained())
		{
			// [체포 추가] 체포 상태만 감지에서 제외한다. 덫의 MOVE_None 상태를 체포로 오인하지 않는다.
			return false;
		}

		const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
		if (IsValid(TargetASC) && TargetASC->HasMatchingGameplayTag(HHTags::Ability_Mimic_GuardDisguise))
		{
			return false;
		}

		return IsWithinVerticalVisionAngle(Actor);
	};

	AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));
	if (const ABaseCharacter* Player = Cast<ABaseCharacter>(CurrentTarget); IsValid(Player) && Player->IsRestrained())
	{
		// 다른 경비가 체포한 대상은 추격 유예·마지막 목격 수색에서도 제외한다.
		// [체포 추가] 후보 목록에서 빼는 것만으로는 기존 TargetActor가 시야 상실 유예에 남는다.
		// 현재 추격도 정리하고 목격·수색 세션을 무효화한 뒤, 아래에서 다른 보이는 대상을 선택한다.
		// 게이지를 비워 이전 대상의 100%가 새 플레이어에게 그대로 넘어가지 않게 한다.
		GuardAIController->StopMovement();
		GuardAIController->ClearFocus(EAIFocusPriority::Gameplay);
		GuardAIController->SetChasing(false);
		BlackboardComp->SetValueAsBool(GuardAIKeys::CanSeeTarget, false);
		BlackboardComp->ClearValue(GuardAIKeys::TargetActor);
		BlackboardComp->ClearValue(GuardAIKeys::LastKnownLocation);
		BlackboardComp->ClearValue(GuardAIKeys::InvestigateLocation);
		BlackboardComp->SetValueAsFloat(GuardAIKeys::DetectionGauge, 0.f);
		BlackboardComp->SetValueAsFloat(GuardAIKeys::LastSeenTime, -100000.f);
		BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, -100000.f);
		GuardAIController->SetAIState(EGuardAIState::Patrol);
		// [체포 추가] 경보 100%라면 SetAIState가 기존 정책에 따라 Patrol 요청을 Search로 바꾼다.
		CurrentTarget = nullptr;
	}
	AActor* VisibleTarget = nullptr;
	if (PerceivedActors.Contains(CurrentTarget) && CanDetectActor(CurrentTarget))
	{
		VisibleTarget = CurrentTarget;
	}
	else
	{
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (AActor* Actor : PerceivedActors)
		{
			if (!CanDetectActor(Actor))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared(GuardCharacter->GetActorLocation(), Actor->GetActorLocation());
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				VisibleTarget = Actor;
			}
		}
	}

	const bool bWasSeeing = BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget);
	const bool bCanSeeTarget = IsValid(VisibleTarget);
	if (bCanSeeTarget)
	{
		// Sight 콜백이 재발화하지 않아도 대상과 목격 정보를 먼저 복구한다.
		BlackboardComp->SetValueAsObject(GuardAIKeys::TargetActor, VisibleTarget);
		BlackboardComp->SetValueAsVector(GuardAIKeys::LastKnownLocation, VisibleTarget->GetActorLocation());
		BlackboardComp->SetValueAsFloat(GuardAIKeys::LastSeenTime, GetWorld()->GetTimeSeconds());

		if (!bWasSeeing || CurrentTarget != VisibleTarget)
		{
			GuardAIController->ClearFocus(EAIFocusPriority::Gameplay);
			if (IsValid(GuardAIController->GuardHearingComp))
			{
				GuardAIController->GuardHearingComp->ClearHearingDebug();
			}

			UE_LOG(LogGuardAI, Log, TEXT("[AI][%s][%s] 대상=%s"), *GetNameSafe(GuardCharacter), IsValid(CurrentTarget) && CurrentTarget != VisibleTarget ? TEXT("타겟 변경") : TEXT("시야 획득"), *GetNameSafe(VisibleTarget));
		}
	}

	// 시야 상실 중에는 마지막 대상을 유지해 기존 추격 유예와 수색 흐름을 보존한다.
	BlackboardComp->SetValueAsBool(GuardAIKeys::CanSeeTarget, bCanSeeTarget);
	if (bWasSeeing && !bCanSeeTarget)
	{
		UE_LOG(LogGuardAI, Log, TEXT("[AI][%s][시야 상실] 대상=%s 게이지=%.0f"), *GetNameSafe(GuardCharacter), *GetNameSafe(CurrentTarget), BlackboardComp->GetValueAsFloat(GuardAIKeys::DetectionGauge));
	}
}



void UGuardSightAComponent::OnTargetPerceptionUpdatedSight(AActor* Actor, FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp)
{
	if (!HasServerAuthority(this) || !IsValid(Actor) || !IsValid(BlackboardComp) || !IsValid(GuardAIController))
	{
		return;
	}

	if (Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>())
	{
		return;
	}

	if (GuardAIController->bDetailedSightSearchLogs)
	{
		UE_LOG(LogGuardAI, Log, TEXT("[AI][%s][시야 콜백] 대상=%s 감지=%s"), *GetNameSafe(GuardAIController->GetPossessGuardPawn()), *GetNameSafe(Actor), Stimulus.WasSuccessfullySensed() ? TEXT("획득") : TEXT("상실"));
	}

	const bool bWasSeeing = BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget);

	// 개별 플레이어의 콜백 값으로 현재 타겟을 덮어쓰지 않는다.
	// 현재 타겟이 실제 시야에 남아 있으면 유지하고, 놓쳤을 때만 다른 감지 대상을 선택한다.
	RefreshSightTarget(BlackboardComp);

	if (BlackboardComp->GetValueAsBool(GuardAIKeys::CanSeeTarget))
	{
		SightLostAtTime = -1.f;
	}
	else if (bWasSeeing)
	{
		SightLostAtTime = GetWorld()->GetTimeSeconds();
		GuardAIController->SetChasing(false);
	}

	// SearchStartTime은 기존대로 게이지 서비스에서 확정 목격 시각을 갱신한다.
}

//경비의 눈 위치 → 플레이어 머리 위치를 기준으로 계산
bool UGuardSightAComponent::IsWithinVerticalVisionAngle(AActor* TargetActor) const
{

	if (!IsValid(TargetActor))
	{
		return false;
	}

	//const AGuardAIController* GuardController = Cast<AGuardAIController>(GetOwner());
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
	const FTransform EyeTransform = GuardCharacter->GetEyeSocketTransform();
	const FVector GuardEyeLocation = EyeTransform.GetLocation();
	//const FVector GuardEyeLocation = GuardCharacter->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, GuardCharacter->GetEyeHeight());
	const FVector TargetHeadLocation = TargetActor->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, GuardCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	const FVector ToTarget = TargetHeadLocation - GuardEyeLocation;
	const FVector Forward = GuardCharacter->GetSightSocketRotation().Vector().GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const float ForwardDistance = FVector::DotProduct(ToTarget, Forward);
	const float VerticalDistance = FVector::DotProduct(ToTarget, FVector::UpVector);
	const float VerticalAngle = FMath::RadiansToDegrees(FMath::Atan2(VerticalDistance, ForwardDistance));

	return FMath::Abs(VerticalAngle) <= VerticalVisionAngleDegrees;

}


void UGuardSightAComponent::DrawSightDebug() const
{
	
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform EyeTransform = GuardCharacter->GetEyeSocketTransform();
	const FVector Origin = EyeTransform.GetLocation();
	// 양안 경계선은 시야 메시와 같은 바닥 높이에 표시하고, 회전/방향은 눈 기준 그대로 둔다.
	const FVector HorizontalOrigin = GuardCharacter->GetActorLocation() + FVector(
		0.0f, 0.0f, -GuardCapsule->GetScaledCapsuleHalfHeight() + 10.0f);

	FVector Forward = GuardCharacter->GetSightSocketRotation().Vector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
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

/*
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

*/

	// ========================================================
	// 수직 시야 경계
	// ========================================================

	{
		const FVector HorizontalForward = Forward.GetSafeNormal2D();
		const FVector UpDirection = (HorizontalForward * FMath::Cos(VerticalAngleRadians) + Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();
		const FVector DownDirection = (HorizontalForward * FMath::Cos(VerticalAngleRadians) - Up * FMath::Sin(VerticalAngleRadians)).GetSafeNormal();

		const FVector UpTraceEnd = Origin + UpDirection * LoseSightRadius;

		FHitResult UpHit;

		const bool bUpBlocked = World->LineTraceSingleByChannel(UpHit, Origin, UpTraceEnd, ECC_Visibility, Params);

		float UpVisibleDistance = bUpBlocked ? FVector::Distance(Origin, UpHit.Location) : LoseSightRadius;

		const bool bUpHitCharacter = bUpBlocked && UpHit.GetActor() && UpHit.GetActor()->IsA<ACharacter>();

		if (bUpHitCharacter)
		{
			UpVisibleDistance = FMath::Min(UpVisibleDistance + CharacterDebugOffset, LoseSightRadius);
		}
		const float UpGreenDistance = FMath::Min(SightRadius, UpVisibleDistance);

		DrawDebugLine(World, Origin, Origin + UpDirection * UpGreenDistance, FColor::Green, false, 0.0f, 0, 4.0f);

		if (UpVisibleDistance > SightRadius)
		{
			DrawDebugLine(World, Origin + UpDirection * SightRadius, Origin + UpDirection * UpVisibleDistance, FColor::Yellow, false, 0.0f, 0, 4.0f);
		}
		if (bUpBlocked)
		{
			const FVector RedStart = bUpHitCharacter ? UpHit.Location + UpDirection * CharacterDebugOffset : UpHit.Location;

			DrawDebugLine(World, RedStart, UpTraceEnd, FColor::Red, false, 0.0f, 0, 2.0f);
		}
	}

/*
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

*/

	{
		// ========================================================
		// 양안 시야 경계
		// ========================================================

		if (BinocularVisionAngleDegrees > 0.0f)
		{
			const float BinocularHalfAngle = BinocularVisionAngleDegrees * 0.5f;

			// 양안 경계선은 바닥 평면에서만 회전시킨다. 소켓의 Pitch/Roll은 적용하지 않는다.
			const FRotator SightYawRotation(0.0f, GuardCharacter->GetSightSocketRotation().Yaw, 0.0f);
			const FVector BinocularLeftDirection = SightYawRotation.RotateVector(
				FVector(FMath::Cos(FMath::DegreesToRadians(BinocularHalfAngle)),
					-FMath::Sin(FMath::DegreesToRadians(BinocularHalfAngle)), 0.0f)).GetSafeNormal();
			const FVector BinocularRightDirection = SightYawRotation.RotateVector(
				FVector(FMath::Cos(FMath::DegreesToRadians(BinocularHalfAngle)),
					FMath::Sin(FMath::DegreesToRadians(BinocularHalfAngle)), 0.0f)).GetSafeNormal();

			DrawDebugLine(World, HorizontalOrigin, HorizontalOrigin + BinocularLeftDirection * SightRadius, FColor::Cyan, false, 0.0f, 0, 2.0f);
			DrawDebugLine(World, HorizontalOrigin, HorizontalOrigin + BinocularRightDirection * SightRadius, FColor::Cyan, false, 0.0f, 0, 2.0f);
		}


	}
}


void UGuardSightAComponent::DrawSightDebugMesh()
{
	// 메시 배열은 보내지 않고, 각 경비의 로컬 표시에서 사용할 작은 값만 갱신한다.
	if (!IsValid(GuardCharacter) || !GuardCharacter->HasAuthority())
	{
		return;
	}
	const EGuardAIState CurrentState = IsValid(GuardAIController) ? GuardAIController->GetAIState() : EGuardAIState::Patrol;
	const bool bIsChasing = CurrentState == EGuardAIState::Chase;
	const FLinearColor FanColor = bIsChasing ? FLinearColor(1.f, 0.35f, 0.f, 1.f) : CurrentState == EGuardAIState::Search ? FLinearColor::Yellow : FLinearColor::White;
	GuardCharacter->SetSightDebugAppearance(bIsChasing, FanColor, SightDebugMaterial);
	GuardCharacter->SetReplicatedSightDebugRotation(FRotator(0.f, GuardCharacter->GetSightSocketRotation().Yaw, 0.f));
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
