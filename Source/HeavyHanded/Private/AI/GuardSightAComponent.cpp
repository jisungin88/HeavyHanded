// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/GuardSightAComponent.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/GuardBlackboardKeys.h"

#include "AI/GuardTypes.h"


// Sets default values for this component's properties
UGuardSightAComponent::UGuardSightAComponent()
{




	// Sight/Hearing 감지 설정은 생성자에서 기본값만 잡는다.
	// 시야각·거리 등 세부 파라미터는 OnPossess -> ApplyGuardStats() 가 DT_GuardStats 에서
	// GuardType 에 맞는 행을 찾아 덮어쓴다. 멤버(UPROPERTY)로 들고 있어야 디테일 패널에도 뜬다.
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));



	// 플레이어는 IGenericTeamAgentInterface를 구현하지 않아 FGenericTeamId::NoTeam(255)로
	// 남는다. 경비 입장에서 그런 상대는 "중립"으로 판정되므로 bDetectNeutrals를 켜야
	// 플레이어를 감지한다. 경비끼리는 위에서 같은 팀으로 묶어 "우호"로 판정되는데,
	// bDetectFriendlies는 꺼서 서로를 감지 대상에서 제외한다 — 켜두면 경비 2명을 배치했을 때
	// 서로를 시야로 잡고 쫓아다니며 교착 상태에 빠진다.
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;


}



// Called when the game starts
void UGuardSightAComponent::BeginPlay()
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
			PerceptionComp->ConfigureSense(*SightConfig);
			SetSightEnabled(bEnableSight);
			///PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UGuardSightComponent::OnTargetPerceptionUpdated);
	}


}


void UGuardSightAComponent::SetSightConfig(float InSightRadius, float InLoseSightRadius, float InPeripheralVisionAngle)
{
	if (!SightConfig)
	{
		// SightConfig 존재하지 않음 로그
		return;
	}

	SightConfig->SightRadius = InSightRadius;
	SightConfig->LoseSightRadius = InLoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = InPeripheralVisionAngle;

}

void UGuardSightAComponent::SetSightEnabled(bool isEnable)
{
	//if (!PerceptionComp) return;

	PerceptionComp->SetSenseEnabled(UAISense_Sight::StaticClass(), bEnableSight);
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
		// 시야를 잃었다고 해서 여기서 SearchStartTime 을 쓰지 않는다.
		//
		// 쓰면 스쳐 지나가듯 한 번 보이기만 해도 조사가 켜진다. Guard.ini 는
		// Guard.State.Investigate 를 "인지 게이지가 가득 차" 진입하는 상태로 정의한다.
		// 그 조건은 BTService_UpdateDetectionGauge 가 게이지 100 인 동안 매 틱
		// SearchStartTime 을 밀어주는 것으로 이미 만족된다 - 시야를 잃는 순간
		// 그 값이 얼어붙어 자연스럽게 "수색 시작 시각"이 된다.
	}

}






/*



// Called every frame
void UGuardSightAComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...

	// 사용시 생성자에 true 필요
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...


}
*/

