// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardHearingAComponent.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"


#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"

#include "AI/GuardTypes.h"


// Sets default values for this component's properties
UGuardHearingAComponent::UGuardHearingAComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...


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

void UGuardHearingAComponent::OnTargetPerceptionUpdatedHearing(AActor* Actor, FAIStimulus Stimulus, UBlackboardComponent* BlackboardComp)
{

	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		// 소실(감지 종료) 이벤트에서는 위치가 유효하지 않을 수 있다.
		// 실제로 소리를 "들은" 순간에만 SoundTargetActor/InvestigateLocation/SearchStartTime을 갱신한다.
		if (Stimulus.WasSuccessfullySensed())
		{
			BlackboardComp->SetValueAsObject(GuardAIKeys::SoundTargetActor, Actor);
			BlackboardComp->SetValueAsVector(GuardAIKeys::InvestigateLocation, Stimulus.StimulusLocation);
			BlackboardComp->SetValueAsFloat(GuardAIKeys::SearchStartTime, GetWorld()->GetTimeSeconds());
		}
	}

}

void UGuardHearingAComponent::SetHearingRange(float InHearingRange)
{
	//if (!HearingConfig) return;

	HearingConfig->HearingRange = InHearingRange;
}

void UGuardHearingAComponent::SetHearingEnabled(bool isEnable)
{
	PerceptionComp->SetSenseEnabled(UAISense_Hearing::StaticClass(), bEnableHearing);
}


// Called when the game starts
void UGuardHearingAComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...


	if (AAIController* AIController = Cast<AAIController>(GetOwner()))
	{
		PerceptionComp = AIController->GetPerceptionComponent();

		if (!PerceptionComp)
		{
			// PerceptionComp 존재하지 않음 로그
			return;
		}
		PerceptionComp->ConfigureSense(*HearingConfig);
		SetHearingEnabled(bEnableHearing);
		///PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UGuardSightComponent::OnTargetPerceptionUpdated);
	}


	
}


// Called every frame
void UGuardHearingAComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

