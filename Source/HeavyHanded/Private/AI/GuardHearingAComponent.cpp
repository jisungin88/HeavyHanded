// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardHearingAComponent.h"

#include "Perception/AIPerceptionComponent.h"

//#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "DrawDebugHelpers.h"

#include "GameFramework/Pawn.h"
#include "AI/GuardAIController.h"
//#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"
#include "Components/CapsuleComponent.h"

#include "AI/GuardTypes.h"
#include "GameplayTagContainer.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

#include "Character/GuardCharacter.h"


// Sets default values for this component's properties
UGuardHearingAComponent::UGuardHearingAComponent()
{

	// Sight/Hearing 감지 설정은 생성자에서 기본값만 잡는다.
	// 시야각·거리 등 세부 파라미터는 OnPossess -> ApplyGuardStats() 가 DT_GuardStats 에서
	// GuardType 에 맞는 행을 찾아 덮어쓴다. 멤버(UPROPERTY)로 들고 있어야 디테일 패널에도 뜬다.
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));


	// 플레이어는 IGenericTeamAgentInterface를 구현하지 않아 FGenericTeamId::NoTeam(255)로
	// 남는다. 경비 입장에서 그런 상대는 "중립"으로 판정되므로 bDetectNeutrals를 켜야
	// 플레이어를 감지한다. 경비끼리는 위에서 같은 팀으로 묶어 "우호"로 판정되는데,
	// bDetectFriendlies는 꺼서 서로를 감지 대상에서 제외한다 — 켜두면 경비 2명을 배치했을 때
	// 서로를 시야로 잡고 쫓아다니며 교착 상태에 빠진다.
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;


}

void UGuardHearingAComponent::InitializeHearingPerception(UAIPerceptionComponent* InPerceptionComp)
{
	PerceptionComp = InPerceptionComp;

	if (IsValid(PerceptionComp) && IsValid(HearingConfig))
	{
		PerceptionComp->ConfigureSense(*HearingConfig);
	}
}

void UGuardHearingAComponent::Initialize(AGuardCharacter* InGuardCharacter, UAIPerceptionComponent* InPerceptionComp)
{
	if (!IsValid(InGuardCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardHearingAComponent Initialize failed: GuardCharacter is invalid."));
		return;
	}
	GuardCharacter = InGuardCharacter;

	if (!IsValid(InPerceptionComp))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardHearingAComponent Initialize failed: PerceptionComp is invalid."));
		return;
	}
	PerceptionComp = InPerceptionComp;

	if (!IsValid(HearingConfig))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardHearingAComponent Initialize failed: SightConfig is invalid."));
		return;
	}

	GuardAIController = Cast<AGuardAIController>(GetOwner());
	if (!IsValid(GuardAIController))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardHearingAComponent Initialize failed: GuardAIController is invalid."));
		return;
	}

	GuardCapsule = GuardCharacter->GetCapsuleComponent();
	if (!IsValid(GuardCapsule))
	{
		UE_LOG(LogTemp, Error, TEXT("GuardSightAComponent Initialize failed: GuardCapsule is invalid."));
		return;
	}





	PerceptionComp->ConfigureSense(*HearingConfig);

	SetHearingEnabled(GuardCharacter->IsHearingEnabled());
	bDrawHearingDebug = GuardCharacter->IsHearingEnabled();
	//SetSightDebugEnabled(GuardCharacter->IsDrawSightDebugEnabled());
}


void UGuardHearingAComponent::OnTargetPerceptionUpdatedHearing(AActor* Actor, FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp)
{

	const FGameplayTag GuardDisguiseTag = FGameplayTag::RequestGameplayTag(FName("Ability.Mimic.GuardDisguise"));

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);

	if (TargetASC && TargetASC->HasMatchingGameplayTag(GuardDisguiseTag))
	{
		return;
	}


	// 일단 게이지가 차기 전엔 의심만
	if (Stimulus.Type != UAISense::GetSenseID<UAISense_Hearing>())
	{
		return;
	}

	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}


	LastHearingLocation = Stimulus.StimulusLocation;
	bHasHearingLocation = true;


	// 현재 추적 대상이 있으면 청각 위치를 바라보지 않는다.
	if (BlackboardComp && BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor))
	{
		return;
	}


	//잠시 비활성화
	//GuardAIController->SetFocalPoint(Stimulus.StimulusLocation);

	
}

void UGuardHearingAComponent::SetHearingRange(float InHearingRange)
{
	HearingConfig->HearingRange = InHearingRange;
}

void UGuardHearingAComponent::SetHearingEnabled(bool isEnable)
{
	PerceptionComp->SetSenseEnabled(UAISense_Hearing::StaticClass(), isEnable);
	PrimaryComponentTick.bCanEverTick = isEnable;
	bDrawHearingDebug = isEnable; // 이미 tick을 끄고 있어서 안해도 상관없음
}

void UGuardHearingAComponent::HandleWorldAlertSilenceTimeout()
{

	if (!GuardAIController->GetPossessGuardPawn() || !GuardAIController->IsWorldAlertSpeedUp())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(WorldAlertSilenceDebugTimerHandle); // 디버그 해제

	GuardAIController->ResetMoveSpeed();

	
	UE_LOG(LogTemp, Warning, TEXT("[GuardSpeed] 무소음 타이머 완료 : Silence Timeout | Pawn = %s | Speed Down | Speed = %.1f"),
		*GuardAIController->GetPossessGuardPawnName(), GuardAIController->GetNormalMoveSpeed());

}

void UGuardHearingAComponent::LogWorldAlertSilenceRemaining()
{
	if (!GetWorld())
	{
		return;
	}

	if (!GuardAIController->GetPossessGuardPawn() || !GuardAIController->IsWorldAlertSpeedUp())
	{
		return;
	}

	const float RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(WorldAlertSilenceTimerHandle);

	if (RemainingTime <= 0.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(WorldAlertSilenceDebugTimerHandle);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[GuardSpeed] 무소음 타이머 | Pawn = %s | 남은 시간 = %.1f sec"),
		*GuardAIController->GetPossessGuardPawnName() , RemainingTime);


}

void UGuardHearingAComponent::StartWorldAlertSilenceTimer()
{
	if (!GetWorld())
	{
		return;
	}

	if (!GuardAIController->GetPossessGuardPawn() || !GuardAIController->IsWorldAlertSpeedUp())
	{
		return;
	}


	GetWorld()->GetTimerManager().ClearTimer(WorldAlertSilenceTimerHandle);
	GetWorld()->GetTimerManager().SetTimer
		(WorldAlertSilenceTimerHandle, this, &UGuardHearingAComponent::HandleWorldAlertSilenceTimeout, GuardAIController->GetWorldAlertSilenceDelay(), false);

	GetWorld()->GetTimerManager().ClearTimer(WorldAlertSilenceDebugTimerHandle);
	GetWorld()->GetTimerManager().SetTimer
		(WorldAlertSilenceDebugTimerHandle, this, &UGuardHearingAComponent::LogWorldAlertSilenceRemaining, 1.0f, true);
}

void UGuardHearingAComponent::ClearWorldAlertSilenceTimer()
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(WorldAlertSilenceTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(WorldAlertSilenceDebugTimerHandle);
}


// Called when the game starts
void UGuardHearingAComponent::BeginPlay()
{
	Super::BeginPlay();

	
}

void UGuardHearingAComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDrawHearingDebug)
	{
		DrawHearingDebug();
	}
}

void UGuardHearingAComponent::DrawHearingDebug() const
{
	//if (!HearingConfig)
	//{
	//	return;
	//}
	//
	//const AGuardAIController* GuardController = Cast<AGuardAIController>(GetOwner());
	//if (!GuardController)
	//{
	//	return;
	//}
	//
	//const APawn* GuardPawn = GuardController->GetPawn();
	//if (!GuardPawn)
	//{
	//	return;
	//}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector HearingOrigin = GuardCharacter->GetActorLocation();
	const FVector HorizontalOrigin = GuardCharacter->GetCapsuleComponent()->GetComponentLocation() - FVector(0.0f, 0.0f, GuardCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	const float HearingRadius = HearingConfig->HearingRange;

	if (HearingRadius <= 0.0f)
	{
		return;
	}

	// 청각 감지 범위
	//DrawDebugSphere(World, HearingOrigin, HearingRadius, 24, FColor::Orange, false, 0.0f, 0, 1.0f);
	DrawDebugCircle(World, HorizontalOrigin, HearingRadius, 24, FColor::Orange, false, 0.0f, 0, 3.0f, FVector::ForwardVector, FVector::RightVector, false);


	// 마지막으로 감지한 소음 위치
	if (bHasHearingLocation)
	{
		DrawDebugSphere(World, LastHearingLocation, 35.0f, 16, FColor::Red, false, 0.0f, 0, 4.0f);
		DrawDebugLine(World, HearingOrigin, LastHearingLocation, FColor::Red, false, 0.0f, 0, 4.0f);
	}

}

void UGuardHearingAComponent::ClearHearingDebug()
{
	bHasHearingLocation = false;
	LastHearingLocation = FVector::ZeroVector;
}

