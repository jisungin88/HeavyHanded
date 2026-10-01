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
	SetComponentTickEnabled(!GetOwner()->HasAuthority());
	if (GetOwner()->HasAuthority())
	{
		GuardCharacter->SetReplicatedSightDebugState(
			SightConfig->SightRadius, SightConfig->PeripheralVisionAngleDegrees, bDrawSightDebug);
	}
	else
	{
		SightConfig->SightRadius = GuardCharacter->GetReplicatedSightRadius();
		SightConfig->PeripheralVisionAngleDegrees = GuardCharacter->GetReplicatedSightHalfAngle();
		bDrawSightDebug = GuardCharacter->GetReplicatedDrawSightDebug();
	}

	SetSightEnabled(GuardCharacter->IsSightEnabled());
	SetSightDebugEnabled(GetOwner()->HasAuthority()
		? GuardCharacter->IsDrawSightDebugEnabled()
		: GuardCharacter->GetReplicatedDrawSightDebug());

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
	if (bDrawSightDebug)
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
	if (!bDrawSightDebug)
	{
		return;
	}

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
			bDrawSightDebug = GuardCharacter->GetReplicatedDrawSightDebug();
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
			SightConfig->SightRadius, SightConfig->PeripheralVisionAngleDegrees, bDrawSightDebug);
		if (bDrawSightDebug)
		{
			DrawSightDebugMesh();
		}
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
	bDrawSightDebug = bInEnabled;
	if (IsValid(GuardCharacter) && GuardCharacter->HasAuthority())
	{
		GuardCharacter->SetReplicatedSightDebugState(
			SightConfig->SightRadius, SightConfig->PeripheralVisionAngleDegrees, bInEnabled);
	}
	if (IsValid(SightDebugMesh))
	{
		SightDebugMesh->SetVisibility(bInEnabled);
	}
	SetComponentTickEnabled(bInEnabled);
	if (bInEnabled && GetOwner()->HasAuthority())
	{
		UpdateSightDebug();
	}
	UpdateSightDebugTimer();

	//if (!bInEnabled)
	//{
	//	SightDebugMesh->ClearAllMeshSections();
	//}



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

	TArray<AActor*> PerceivedActors;
	if (GuardCharacter->IsSightEnabled())
	{
		PerceptionComp->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
	}

	const auto CanDetectActor = [this](AActor* Actor)
	{
		if (!IsValid(Actor) || !IsValid(Actor->GetRootComponent()))
		{
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

			UE_LOG(LogGuardAI, Log, TEXT("[%s] 현재 시야 대상 복구: %s"), *GetNameSafe(GuardCharacter), *GetNameSafe(VisibleTarget));
		}
	}

	// 시야 상실 중에는 마지막 대상을 유지해 기존 추격 유예와 수색 흐름을 보존한다.
	BlackboardComp->SetValueAsBool(GuardAIKeys::CanSeeTarget, bCanSeeTarget);
}



void UGuardSightAComponent::OnTargetPerceptionUpdatedSight
		(AActor* Actor, FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp)
{
	if (!HasServerAuthority(this))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);

	if (TargetASC && TargetASC->HasMatchingGameplayTag(HHTags::Ability_Mimic_GuardDisguise))
	{
		return;
	}


	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{

		UE_LOG(LogGuardAI, Warning, TEXT("[%s] Sight 콜백: Actor=%s Sensed=%d AIState=%d"),
			*GetNameSafe(GuardAIController->GetPossessGuardPawn()), *GetNameSafe(Actor), Stimulus.WasSuccessfullySensed(), static_cast<int32>(GuardAIController->GetAIState()));


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

			// 추격 속도 설정
			if (GuardAIController)
			{
				GuardAIController->SetChasing(false);
			}

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


/*
//작동시삭제0929
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
	const FRotator EyeRotation = GuardCharacter->GetSightSocketRotation();
	const FRotator SightMeshRotation(0.0f, EyeRotation.Yaw, 0.0f);
	SightDebugMesh->SetWorldRotation(SightMeshRotation);
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
*/

void UGuardSightAComponent::DrawSightDebugMesh()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(SightDebugMesh) || !IsValid(GuardCapsule) || !IsValid(SightConfig) || !IsValid(GuardCharacter))
	{
		return;
	}

	const float SightRadius = SightConfig->SightRadius;
	const float HalfAngle = SightConfig->PeripheralVisionAngleDegrees;
	if (SightRadius <= 0.0f || HalfAngle <= 0.0f)
	{
		SightDebugMesh->ClearAllMeshSections();
		return;
	}

	constexpr int32 ArcSegments = 96;
	const int32 AngularPointCount = ArcSegments + 1;

	const float CapsuleHalfHeight = GuardCapsule->GetScaledCapsuleHalfHeight();
	const FVector LocalOrigin(0.0f, 0.0f, -CapsuleHalfHeight + 2.0f);
	const FRotator EyeRotation = GuardCharacter->GetSightSocketRotation();
	const FRotator SightMeshRotation(0.0f, EyeRotation.Yaw, 0.0f);
	SightDebugMesh->SetWorldRotation(SightMeshRotation);
	const FTransform MeshTransform = SightDebugMesh->GetComponentTransform();
	const FVector WorldOrigin = MeshTransform.TransformPosition(LocalOrigin);
	const FVector UpDirection = FVector::UpVector;
	constexpr float MinGroundNormalZ = 0.7f;
	const FVector EyeLocation(
		WorldOrigin.X,
		WorldOrigin.Y,
		GuardCharacter->GetRootComponent()->GetComponentLocation().Z + GuardCharacter->GetEyeHeight());

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSightDebug), true, GuardCharacter);
	AGuardAIController* GuardController = Cast<AGuardAIController>(GetOwner());
	const EGuardAIState CurrentState = IsValid(GuardController)
		? GuardController->GetAIState()
		: EGuardAIState::Patrol;
	const bool bIsChasing = IsValid(GuardController) &&
		CurrentState == EGuardAIState::Chase;
	if (bIsChasing)
	{
		if (UBlackboardComponent* BlackboardComp = GuardController->GetBlackboardComponent())
		{
			AActor* CurrentTarget = Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor));
			if (IsValid(CurrentTarget))
			{
				Params.AddIgnoredActor(CurrentTarget);
			}
		}
	}

	// 눈높이보다 낮은 길과 턱은 건너뛰고, 눈높이에 닿는 장애물만 찾는다.
	TArray<float> VisibleDistances;
	VisibleDistances.Reserve(AngularPointCount);

	for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
	{
		const float Alpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
		const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, Alpha);
		const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
		const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
		const FVector TraceEnd = WorldOrigin + WorldDirection * SightRadius;
		FCollisionQueryParams AngleParams = Params;
		float VisibleDistance = SightRadius;

		// 눈높이보다 낮은 장애물은 건너뛰고, 눈높이에 닿는 장애물에서만 자른다.
		while (true)
		{
			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(
				Hit, WorldOrigin, TraceEnd, ECC_Visibility, AngleParams);
			if (!bBlocked)
			{
				break;
			}

			const float HitDistance = FMath::Clamp(
				FVector::DotProduct(Hit.Location - WorldOrigin, WorldDirection), 0.0f, SightRadius);
			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!IsValid(HitComponent))
			{
				VisibleDistance = HitDistance;
				break;
			}

			if (Hit.ImpactNormal.Z >= MinGroundNormalZ)
			{
				AngleParams.AddIgnoredComponent(HitComponent);
				continue;
			}

			const FVector BoundsOrigin = HitComponent->Bounds.Origin;
			const FVector BoundsExtent = HitComponent->Bounds.BoxExtent;
			const float ObstacleTopZ = BoundsOrigin.Z + BoundsExtent.Z;

			// 눈높이보다 낮은 길과 턱은 부채꼴을 자르지 않는다.
			// 눈높이에 닿는 벽과 건물부터 시야 외곽을 자른다.
			const bool bBlocksSightFan = ObstacleTopZ >= EyeLocation.Z;

			if (bBlocksSightFan)
			{
				VisibleDistance = HitDistance;
				if (Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>())
				{
					VisibleDistance = FMath::Min(VisibleDistance + 50.0f, SightRadius);
				}
				break;
			}

			AngleParams.AddIgnoredComponent(HitComponent);
		}

		VisibleDistances.Add(VisibleDistance);
	}

	/*
	//작동시삭제0929
	// 중심, 중간, 바깥쪽 정점을 지면에 투영해 낮은 길 위에서도 메시가 묻히지 않게 한다.
	constexpr int32 RadialSegments = 2;
	constexpr float GroundTraceUp = 1000.0f;
	constexpr float GroundTraceDown = 5000.0f;
	constexpr float GroundSurfaceOffset = 2.0f;
	const FVector FlatMeshOrigin(0.0f, 0.0f, -CapsuleHalfHeight + 10.0f);
	const FVector FlatMeshOriginWorld = MeshTransform.TransformPosition(FlatMeshOrigin);

	TArray<FVector> SurfaceVertices;
	TArray<int32> SurfaceTriangles;
	TArray<FVector> SurfaceNormals;
	TArray<FVector2D> SurfaceUV0;
	TArray<FLinearColor> SurfaceVertexColors;
	TArray<FProcMeshTangent> SurfaceTangents;
	const int32 SurfaceVertexCount = 1 + RadialSegments * AngularPointCount;
	SurfaceVertices.Reserve(SurfaceVertexCount);
	SurfaceTriangles.Reserve(ArcSegments * RadialSegments * 6);
	SurfaceNormals.Reserve(SurfaceVertexCount);
	SurfaceUV0.Reserve(SurfaceVertexCount);
	SurfaceVertexColors.Reserve(SurfaceVertexCount);

	auto AddSurfaceVertex = [&](const FVector& SampleWorldPoint, const FVector2D& UV)
	{
		const FVector TraceStart = SampleWorldPoint + UpDirection * GroundTraceUp;
		const FVector TraceEnd = SampleWorldPoint - UpDirection * GroundTraceDown;
		FHitResult GroundHit;
		const bool bGroundTraceHit = World->LineTraceSingleByChannel(
			GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params);
		const AActor* GroundActor = GroundHit.GetActor();
		const bool bUseGroundPoint = bGroundTraceHit && GroundHit.ImpactNormal.Z >= MinGroundNormalZ &&
			(!IsValid(GroundActor) || !GroundActor->IsA<ACharacter>());

		const FVector VertexWorldPoint = bUseGroundPoint
			? GroundHit.ImpactPoint + UpDirection * GroundSurfaceOffset
			: FVector(SampleWorldPoint.X, SampleWorldPoint.Y, FlatMeshOriginWorld.Z);
		const FVector VertexWorldNormal = bUseGroundPoint ? GroundHit.ImpactNormal : UpDirection;
		SurfaceVertices.Add(MeshTransform.InverseTransformPosition(VertexWorldPoint));
		SurfaceNormals.Add(MeshTransform.InverseTransformVectorNoScale(VertexWorldNormal).GetSafeNormal());
		SurfaceUV0.Add(UV);
		SurfaceVertexColors.Add(FLinearColor::White);
	};

	AddSurfaceVertex(WorldOrigin, FVector2D::ZeroVector);
	for (int32 RadialIndex = 1; RadialIndex <= RadialSegments; ++RadialIndex)
	{
		const float RadialAlpha = static_cast<float>(RadialIndex) / static_cast<float>(RadialSegments);
		for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
		{
			const float AngleAlpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
			const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, AngleAlpha);
			const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
			const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
			const float Distance = VisibleDistances[AngleIndex] * RadialAlpha;
			const FVector SampleWorldPoint = WorldOrigin + WorldDirection * Distance;
			AddSurfaceVertex(SampleWorldPoint, FVector2D(RadialAlpha, AngleAlpha));
		}
	}

	for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
	{
		const int32 CurrentIndex = 1 + AngleIndex;
		const int32 NextIndex = CurrentIndex + 1;
		SurfaceTriangles.Add(0);
		SurfaceTriangles.Add(NextIndex);
		SurfaceTriangles.Add(CurrentIndex);
	}

	for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
	{
		const int32 InnerCurrent = 1 + AngleIndex;
		const int32 InnerNext = InnerCurrent + 1;
		const int32 OuterCurrent = 1 + AngularPointCount + AngleIndex;
		const int32 OuterNext = OuterCurrent + 1;
		SurfaceTriangles.Add(OuterNext);
		SurfaceTriangles.Add(OuterCurrent);
		SurfaceTriangles.Add(InnerCurrent);
		SurfaceTriangles.Add(OuterNext);
		SurfaceTriangles.Add(InnerCurrent);
		SurfaceTriangles.Add(InnerNext);
	}
	*/

	// 평면 부채꼴은 발밑보다 조금 위에 두어 항상 깔끔한 형태를 유지한다.
	constexpr float FlatMeshHeightOffset = 10.0f;
	const FVector FlatMeshOrigin(0.0f, 0.0f, -CapsuleHalfHeight + FlatMeshHeightOffset);

	TArray<FVector> FlatVertices;
	TArray<int32> FlatTriangles;
	TArray<FVector> FlatNormals;
	TArray<FVector2D> FlatUV0;
	TArray<FLinearColor> FlatVertexColors;
	TArray<FProcMeshTangent> FlatTangents;
	FlatVertices.Reserve(AngularPointCount + 1);
	FlatTriangles.Reserve(ArcSegments * 3);
	FlatNormals.Reserve(AngularPointCount + 1);
	FlatUV0.Reserve(AngularPointCount + 1);
	FlatVertexColors.Reserve(AngularPointCount + 1);
	const FLinearColor FlatFanColor = bIsChasing
		? FLinearColor(1.0f, 0.35f, 0.0f, 1.0f)
		: CurrentState == EGuardAIState::Search ? FLinearColor::Yellow : FLinearColor::White;

	FlatVertices.Add(FlatMeshOrigin);
	FlatNormals.Add(FVector::UpVector);
	FlatUV0.Add(FVector2D::ZeroVector);
	FlatVertexColors.Add(FlatFanColor);

	for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
	{
		const float AngleAlpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
		const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, AngleAlpha);
		const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
		FlatVertices.Add(FlatMeshOrigin + LocalDirection * VisibleDistances[AngleIndex]);
		FlatNormals.Add(FVector::UpVector);
		FlatUV0.Add(FVector2D::ZeroVector);
		FlatVertexColors.Add(FlatFanColor);
	}

	for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
	{
		FlatTriangles.Add(0);
		FlatTriangles.Add(AngleIndex + 2);
		FlatTriangles.Add(AngleIndex + 1);
	}

	TArray<FVector> GroundVertices;
	TArray<int32> GroundTriangles;
	TArray<FVector> GroundNormals;
	TArray<FVector2D> GroundUV0;
	TArray<FLinearColor> GroundVertexColors;
	TArray<FProcMeshTangent> GroundTangents;
	bool bHasGroundOverlay = false;

	if (!bIsChasing)
	{
		// 낮은 길의 윗면은 평면 부채꼴과 별도 메시로 만든다.
		// 서로 다른 높이의 바닥을 연결하지 않아 경사 삼각형이 생기지 않는다.
		constexpr float GroundTraceUp = 1000.0f;
		constexpr float GroundTraceDown = 5000.0f;
		constexpr float GroundSurfaceOffset = 2.0f;
		// 길 표면을 더 촘촘히 샘플링해 큰 삼각형 조각을 줄인다.
		constexpr float GroundSampleSpacing = 100.0f;
		// 같은 평면에 가까운 정점만 연결한다.
		// 낮은 길의 경계는 끊기므로 바닥으로 내려가는 경사 삼각형이 생기지 않는다.
		constexpr float FlatSurfaceNormalZ = 0.98f;
		constexpr float MaxOverlayHeightDelta = 5.0f;
		const int32 GroundRadialSegments = FMath::Clamp(FMath::CeilToInt(SightRadius / GroundSampleSpacing), 4, 24);
		const FVector FlatMeshOriginWorld = MeshTransform.TransformPosition(FlatMeshOrigin);

		struct FGroundOverlaySample
		{
			FVector SampleWorldPoint = FVector::ZeroVector;
			FVector SurfaceWorldPoint = FVector::ZeroVector;
			FVector SurfaceWorldNormal = FVector::UpVector;
			float SurfaceHeight = 0.0f;
			bool bIsValid = false;
		};

		TArray<FGroundOverlaySample> GroundOverlaySamples;
		const int32 GroundVertexCount = (GroundRadialSegments + 1) * AngularPointCount;
		GroundOverlaySamples.Reserve(GroundVertexCount);
		const int32 GroundCellCount = GroundRadialSegments * ArcSegments;
		GroundVertices.Reserve(GroundCellCount * 8);
		GroundNormals.Reserve(GroundCellCount * 8);
		GroundUV0.Reserve(GroundCellCount * 8);
		GroundVertexColors.Reserve(GroundCellCount * 8);
		GroundTriangles.Reserve(GroundCellCount * 12);

		auto QueryGroundOverlaySample = [&](const FVector& SampleWorldPoint)
		{
			FGroundOverlaySample Sample;
			Sample.SampleWorldPoint = SampleWorldPoint;
			const FVector TraceStart = SampleWorldPoint + UpDirection * GroundTraceUp;
			const FVector TraceEnd = SampleWorldPoint - UpDirection * GroundTraceDown;
			FHitResult GroundHit;
			const bool bGroundTraceHit = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params);
			const AActor* GroundActor = GroundHit.GetActor();
			const bool bIsWalkableGround = bGroundTraceHit && GroundHit.ImpactNormal.Z >= MinGroundNormalZ &&
				(!IsValid(GroundActor) || !GroundActor->IsA<ACharacter>());
			const bool bIsFlatSurface = bIsWalkableGround && GroundHit.ImpactNormal.Z >= FlatSurfaceNormalZ;
			const bool bIsAboveFlatMesh = bIsWalkableGround &&
				GroundHit.ImpactPoint.Z > FlatMeshOriginWorld.Z + GroundSurfaceOffset;
			const bool bIsBelowEyeHeight = bIsWalkableGround && GroundHit.ImpactPoint.Z < EyeLocation.Z;
			Sample.bIsValid = bIsFlatSurface && bIsAboveFlatMesh && bIsBelowEyeHeight;
			if (Sample.bIsValid)
			{
				Sample.SurfaceWorldPoint = GroundHit.ImpactPoint + UpDirection * GroundSurfaceOffset;
				Sample.SurfaceWorldNormal = GroundHit.ImpactNormal;
				Sample.SurfaceHeight = GroundHit.ImpactPoint.Z;
			}
			return Sample;
		};

		for (int32 RadialIndex = 0; RadialIndex <= GroundRadialSegments; ++RadialIndex)
		{
			const float RadialAlpha = static_cast<float>(RadialIndex) / static_cast<float>(GroundRadialSegments);
			for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
			{
				const float AngleAlpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
				const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, AngleAlpha);
				const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
				const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
				const float Distance = VisibleDistances[AngleIndex] * RadialAlpha;
				const FVector SampleWorldPoint = WorldOrigin + WorldDirection * Distance;
				GroundOverlaySamples.Add(QueryGroundOverlaySample(SampleWorldPoint));
			}
		}

		// 삼각형 단위로 유효한 지면 경계를 찾아 잘라낸다.
		TArray<FGroundOverlaySample> ClippedTriangle;
		ClippedTriangle.Reserve(4);
		TMap<uint64, FGroundOverlaySample> BoundarySampleCache;
		BoundarySampleCache.Reserve(GroundCellCount * 2);
		auto AddClippedGroundTriangle = [&](int32 FirstIndex, int32 SecondIndex, int32 ThirdIndex)
		{
			const int32 TriangleIndices[] = { FirstIndex, SecondIndex, ThirdIndex };
			ClippedTriangle.Reset();
			for (int32 EdgeIndex = 0; EdgeIndex < UE_ARRAY_COUNT(TriangleIndices); ++EdgeIndex)
			{
				const int32 EdgeStartIndex = TriangleIndices[EdgeIndex];
				const int32 EdgeEndIndex = TriangleIndices[(EdgeIndex + 1) % UE_ARRAY_COUNT(TriangleIndices)];
				const FGroundOverlaySample& EdgeStart = GroundOverlaySamples[EdgeStartIndex];
				const FGroundOverlaySample& EdgeEnd = GroundOverlaySamples[EdgeEndIndex];

				if (EdgeStart.bIsValid)
				{
					ClippedTriangle.Add(EdgeStart);
				}

				if (EdgeStart.bIsValid == EdgeEnd.bIsValid)
				{
					continue;
				}

				const uint64 BoundaryCacheKey =
					(static_cast<uint64>(FMath::Min(EdgeStartIndex, EdgeEndIndex)) << 32) |
					static_cast<uint64>(FMath::Max(EdgeStartIndex, EdgeEndIndex));
				if (const FGroundOverlaySample* CachedSample = BoundarySampleCache.Find(BoundaryCacheKey))
				{
					ClippedTriangle.Add(*CachedSample);
					continue;
				}

				float ValidAlpha = EdgeStart.bIsValid ? 0.0f : 1.0f;
				float InvalidAlpha = EdgeStart.bIsValid ? 1.0f : 0.0f;
				FGroundOverlaySample BoundarySample = EdgeStart.bIsValid ? EdgeStart : EdgeEnd;
				for (int32 SubdivisionIndex = 0; SubdivisionIndex < 8; ++SubdivisionIndex)
				{
					const float TestAlpha = (ValidAlpha + InvalidAlpha) * 0.5f;
					const FVector TestWorldPoint = FMath::Lerp(
						EdgeStart.SampleWorldPoint, EdgeEnd.SampleWorldPoint, TestAlpha);
					const FGroundOverlaySample TestSample = QueryGroundOverlaySample(TestWorldPoint);
					if (TestSample.bIsValid)
					{
						ValidAlpha = TestAlpha;
						BoundarySample = TestSample;
					}
					else
					{
						InvalidAlpha = TestAlpha;
					}
				}
				BoundarySampleCache.Add(BoundaryCacheKey, BoundarySample);
				ClippedTriangle.Add(BoundarySample);
			}

			if (ClippedTriangle.Num() < 3)
			{
				return;
			}

			float MinHeight = ClippedTriangle[0].SurfaceHeight;
			float MaxHeight = MinHeight;
			for (const FGroundOverlaySample& TriangleSample : ClippedTriangle)
			{
				MinHeight = FMath::Min(MinHeight, TriangleSample.SurfaceHeight);
				MaxHeight = FMath::Max(MaxHeight, TriangleSample.SurfaceHeight);
			}
			if (MaxHeight - MinHeight > MaxOverlayHeightDelta)
			{
				return;
			}

			const int32 TriangleVertexStart = GroundVertices.Num();
			for (const FGroundOverlaySample& TriangleSample : ClippedTriangle)
			{
				GroundVertices.Add(MeshTransform.InverseTransformPosition(TriangleSample.SurfaceWorldPoint));
				GroundNormals.Add(MeshTransform.InverseTransformVectorNoScale(TriangleSample.SurfaceWorldNormal).GetSafeNormal());
				GroundUV0.Add(FVector2D::ZeroVector);
				GroundVertexColors.Add(FlatFanColor);
			}

			for (int32 TriangleIndex = 1; TriangleIndex < ClippedTriangle.Num() - 1; ++TriangleIndex)
			{
				GroundTriangles.Add(TriangleVertexStart);
				GroundTriangles.Add(TriangleVertexStart + TriangleIndex);
				GroundTriangles.Add(TriangleVertexStart + TriangleIndex + 1);
			}
		};

		for (int32 RadialIndex = 1; RadialIndex <= GroundRadialSegments; ++RadialIndex)
		{
			const int32 InnerBase = (RadialIndex - 1) * AngularPointCount;
			const int32 OuterBase = RadialIndex * AngularPointCount;
			for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
			{
				const int32 InnerCurrent = InnerBase + AngleIndex;
				const int32 InnerNext = InnerCurrent + 1;
				const int32 OuterCurrent = OuterBase + AngleIndex;
				const int32 OuterNext = OuterCurrent + 1;
				AddClippedGroundTriangle(InnerCurrent, InnerNext, OuterNext);
				AddClippedGroundTriangle(OuterNext, OuterCurrent, InnerCurrent);
			}
		}

		bHasGroundOverlay = GroundTriangles.Num() > 0;
	}

	/*
	//작동시삭제0929
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	TArray<uint8> bGroundVertexValid;
	TArray<float> GroundHeights;

	const int32 GroundVertexCount = 1 + RadialSegments * AngularPointCount;
	Vertices.Reserve(GroundVertexCount);
	Normals.Reserve(GroundVertexCount);
	UV0.Reserve(GroundVertexCount);
	VertexColors.Reserve(GroundVertexCount);
	bGroundVertexValid.Reserve(GroundVertexCount);
	GroundHeights.Reserve(GroundVertexCount);
	Triangles.Reserve(ArcSegments * RadialSegments * 6);

	// 지면을 못 찾은 정점은 삼각형 생성에서 제외한다.
	auto AddGroundVertex = [&](const FVector& SampleWorldPoint, const FVector2D& UV)
	{
		const FVector TraceStart = SampleWorldPoint + UpDirection * GroundTraceUp;
		const FVector TraceEnd = SampleWorldPoint - UpDirection * GroundTraceDown;
		FHitResult GroundHit;
		const bool bGroundTraceHit = World->LineTraceSingleByChannel(
			GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params);
		const AActor* GroundActor = GroundHit.GetActor();
		const bool bHeightWithinRange = bGroundTraceHit &&
			FMath::Abs(GroundHit.ImpactPoint.Z - GroundReferenceZ) <= MaxGroundHeightOffset;
		const bool bHasGround = bGroundTraceHit && GroundHit.ImpactNormal.Z >= MinGroundNormalZ &&
			bHeightWithinRange &&
			(!IsValid(GroundActor) || !GroundActor->IsA<ACharacter>());

		if (bHasGround)
		{
			const FVector GroundPoint = GroundHit.ImpactPoint + UpDirection * GroundSurfaceOffset;
			Vertices.Add(MeshTransform.InverseTransformPosition(GroundPoint));
			Normals.Add(MeshTransform.InverseTransformVectorNoScale(GroundHit.ImpactNormal).GetSafeNormal());
			bGroundVertexValid.Add(1);
			GroundHeights.Add(GroundHit.ImpactPoint.Z);
		}
		else
		{
			Vertices.Add(MeshTransform.InverseTransformPosition(SampleWorldPoint));
			Normals.Add(FVector::UpVector);
			bGroundVertexValid.Add(0);
			GroundHeights.Add(SampleWorldPoint.Z);
		}

		UV0.Add(UV);
		VertexColors.Add(FLinearColor::White);
	};

	FHitResult CenterGroundHit;
	const bool bCenterGroundTraceHit = World->LineTraceSingleByChannel(
		CenterGroundHit,
		WorldOrigin + UpDirection * GroundTraceUp,
		WorldOrigin - UpDirection * GroundTraceDown,
		ECC_Visibility,
		Params);
	const AActor* CenterGroundActor = CenterGroundHit.GetActor();
	const bool bCenterHeightWithinRange = bCenterGroundTraceHit &&
		FMath::Abs(CenterGroundHit.ImpactPoint.Z - GroundReferenceZ) <= MaxGroundHeightOffset;
	const bool bHasCenterGround = bCenterGroundTraceHit && CenterGroundHit.ImpactNormal.Z >= MinGroundNormalZ &&
		bCenterHeightWithinRange &&
		(!IsValid(CenterGroundActor) || !CenterGroundActor->IsA<ACharacter>());

	if (!bHasCenterGround)
	{
		SightDebugMesh->ClearAllMeshSections();
		return;
	}

	const FVector CenterGroundPoint = CenterGroundHit.ImpactPoint + UpDirection * GroundSurfaceOffset;
	Vertices.Add(MeshTransform.InverseTransformPosition(CenterGroundPoint));
	Normals.Add(MeshTransform.InverseTransformVectorNoScale(CenterGroundHit.ImpactNormal).GetSafeNormal());
	UV0.Add(FVector2D::ZeroVector);
	VertexColors.Add(FLinearColor::White);
	bGroundVertexValid.Add(1);
	GroundHeights.Add(CenterGroundHit.ImpactPoint.Z);

	// 발밑에서 살짝 띄워 보여줄 평면 부채꼴 메시를 준비한다.
	TArray<FVector> FlatVertices;
	TArray<int32> FlatTriangles;
	TArray<FVector> FlatNormals;
	TArray<FVector2D> FlatUV0;
	TArray<FLinearColor> FlatVertexColors;
	TArray<FProcMeshTangent> FlatTangents;
	const FVector FlatMeshOrigin(0.0f, 0.0f, -CapsuleHalfHeight + 5.0f);
	FlatVertices.Reserve(AngularPointCount + 1);
	FlatTriangles.Reserve(ArcSegments * 3);
	FlatNormals.Reserve(AngularPointCount + 1);
	FlatUV0.Reserve(AngularPointCount + 1);
	FlatVertexColors.Reserve(AngularPointCount + 1);
	FlatVertices.Add(FlatMeshOrigin);
	FlatNormals.Add(FVector::UpVector);
	FlatUV0.Add(FVector2D::ZeroVector);
	FlatVertexColors.Add(FLinearColor::White);

	for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
	{
		const float AngleAlpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
		const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, AngleAlpha);
		const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
		FlatVertices.Add(FlatMeshOrigin + LocalDirection * VisibleDistances[AngleIndex]);
		FlatNormals.Add(FVector::UpVector);
		FlatUV0.Add(FVector2D::ZeroVector);
		FlatVertexColors.Add(FLinearColor::White);
	}

	for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
	{
		FlatTriangles.Add(0);
		FlatTriangles.Add(AngleIndex + 2);
		FlatTriangles.Add(AngleIndex + 1);
	}

	// 각도별 시야 거리 안쪽을 여러 반경으로 나누고, 각 지점을 지면에 투영한다.
	for (int32 RadialIndex = 1; RadialIndex <= RadialSegments; ++RadialIndex)
	{
		const float RadialAlpha = static_cast<float>(RadialIndex) / static_cast<float>(RadialSegments);

		for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
		{
			const float AngleAlpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
			const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, AngleAlpha);
			const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
			const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
			float Distance = VisibleDistances[AngleIndex] * RadialAlpha;
			if (bBlockedByObstacle[AngleIndex] && RadialIndex == RadialSegments)
			{
				Distance = FMath::Max(0.0f, Distance - WallEdgeInset);
			}

			const FVector SampleWorldPoint = WorldOrigin + WorldDirection * Distance;

			AddGroundVertex(SampleWorldPoint, FVector2D(RadialAlpha, AngleAlpha));
		}
	}

	// 중심과 첫 번째 지면 링을 연결한다.
	for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
	{
		const int32 CurrentIndex = 1 + AngleIndex;
		const int32 NextIndex = CurrentIndex + 1;
		const float MinHeight = FMath::Min3(
			GroundHeights[0], GroundHeights[CurrentIndex], GroundHeights[NextIndex]);
		const float MaxHeight = FMath::Max3(
			GroundHeights[0], GroundHeights[CurrentIndex], GroundHeights[NextIndex]);
		if (bGroundVertexValid[0] && bGroundVertexValid[CurrentIndex] && bGroundVertexValid[NextIndex] &&
			MaxHeight - MinHeight <= MaxTriangleHeightDelta)
		{
			Triangles.Add(0);
			Triangles.Add(NextIndex);
			Triangles.Add(CurrentIndex);
		}
	}

	// 인접한 지면 링 사이를 사각형 두 개의 삼각형으로 연결한다.
	for (int32 RadialIndex = 2; RadialIndex <= RadialSegments; ++RadialIndex)
	{
		const int32 InnerBase = 1 + (RadialIndex - 2) * AngularPointCount;
		const int32 OuterBase = 1 + (RadialIndex - 1) * AngularPointCount;

		for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
		{
			const int32 InnerCurrent = InnerBase + AngleIndex;
			const int32 InnerNext = InnerCurrent + 1;
			const int32 OuterCurrent = OuterBase + AngleIndex;
			const int32 OuterNext = OuterCurrent + 1;
			const float MinHeight = FMath::Min(
				FMath::Min(GroundHeights[InnerCurrent], GroundHeights[InnerNext]),
				FMath::Min(GroundHeights[OuterCurrent], GroundHeights[OuterNext]));
			const float MaxHeight = FMath::Max(
				FMath::Max(GroundHeights[InnerCurrent], GroundHeights[InnerNext]),
				FMath::Max(GroundHeights[OuterCurrent], GroundHeights[OuterNext]));

			if (bGroundVertexValid[InnerCurrent] && bGroundVertexValid[InnerNext] &&
				bGroundVertexValid[OuterCurrent] && bGroundVertexValid[OuterNext] &&
				MaxHeight - MinHeight <= MaxTriangleHeightDelta)
			{
				Triangles.Add(OuterNext);
				Triangles.Add(OuterCurrent);
				Triangles.Add(InnerCurrent);
				Triangles.Add(OuterNext);
				Triangles.Add(InnerCurrent);
				Triangles.Add(InnerNext);
			}
		}
	}

	*/

	SightDebugMesh->ClearAllMeshSections();
	SightDebugMesh->CreateMeshSection_LinearColor(
		0,
		FlatVertices,
		FlatTriangles,
		FlatNormals,
		FlatUV0,
		FlatVertexColors,
		FlatTangents,
		false);

	if (bHasGroundOverlay)
	{
		SightDebugMesh->CreateMeshSection_LinearColor(
			1,
			GroundVertices,
			GroundTriangles,
			GroundNormals,
			GroundUV0,
			GroundVertexColors,
			GroundTangents,
			false);
	}

	SightDebugMesh->SetVisibility(true);
	SightDebugMesh->SetHiddenInGame(false);
	SightDebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (SightDebugMaterial)
	{
		SightDebugMesh->SetMaterial(0, SightDebugMaterial);
		if (bHasGroundOverlay)
		{
			SightDebugMesh->SetMaterial(1, SightDebugMaterial);
		}
	}

	if (IsValid(GuardCharacter) && GuardCharacter->HasAuthority())
	{
		GuardCharacter->Multicast_UpdateSightDebugMesh(
			FlatVertices, FlatTriangles, GroundVertices, GroundTriangles, SightMeshRotation, FlatFanColor, SightDebugMaterial);
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

