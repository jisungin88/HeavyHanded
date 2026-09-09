#include "Character/GuardCharacter.h"
#include "AI/GuardTypes.h"

//#include "Noise/PerceptionMeterComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


#include "UI/DetectionGaugeWidget.h"
#include "UI/PerceptionMeterWidget.h"

#include "Kismet/GameplayStatics.h"
#include "AI/GuardAIController.h"


AGuardCharacter::AGuardCharacter()
{
	//???? < AI 컨트롤러에도 있음
	// PerceptionMeter = CreateDefaultSubobject<UPerceptionMeterComponent>(TEXT("PerceptionMeter"));

	// 팀 어피니에이션(GuardAIController::SetGenericTeamId)으로 서로를 "감지"는 안 하게 됐지만,
	// 순찰 경로가 겹치면 캡슐끼리 물리적으로 계속 밀며 그 자리에 멈춰(마주보는 것처럼 보임) 있고,
	// 그 상태에서는 PatrolArrivalRadius 안으로 못 들어와 다음 순찰 지점으로도 못 넘어간다.
	// RVO Avoidance를 켜서 서로를 스쳐 지나가도록 미리 피하게 한다.
	GetCharacterMovement()->bUseRVOAvoidance = true;

	// Screen space로 두면 항상 카메라를 향해 평면으로 그려지므로(빌보드), World space처럼
	// 경비가 돌아설 때 게이지가 옆으로 눕는 문제가 없다. 위치만 머리 위로 올려서 붙인다.
	DetectionGaugeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DetectionGaugeWidgetComponent"));
	DetectionGaugeWidgetComponent->SetupAttachment(GetCapsuleComponent());
	DetectionGaugeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DetectionGaugeWidgetComponent->SetDrawSize(FVector2D(120.f, 16.f));
	DetectionGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	// 위젯 클래스는 여기서 강제하지 않는다 - BP_GuardBase 등 파생 BP에서
	// 컴포넌트 디테일 패널의 Widget Class로 WBP_DetectionGauge를 지정할 것.



	// 소리 디버그용 위젯
	HearingGaugeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NoiseGaugeWidgetComponent"));
	HearingGaugeWidgetComponent->SetupAttachment(GetCapsuleComponent());
	HearingGaugeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HearingGaugeWidgetComponent->SetDrawSize(FVector2D(120.f, 16.f));
	HearingGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 135.f));
	// HearingGaugeWidgetComponent->SetDrawAtDesiredSize(false);

}

void AGuardCharacter::BeginPlay()
{
	Super::BeginPlay();

	UPerceptionMeterWidget* PerceptionWidget = Cast<UPerceptionMeterWidget>(HearingGaugeWidgetComponent->GetUserWidgetObject());
	if (!PerceptionWidget)
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] HearingGaugeWidgetComponent에서 PerceptionMeterWidget을 가져오지 못했습니다."), *GetName());
		return;
	}

	UE_LOG(LogGuardAI, Warning, TEXT("[%s] PerceptionMeterWidget 연결 성공."), *GetName());
	PerceptionWidget->BindToGuard(this);



	if (HearingGaugeWidgetComponent)
	{
		UUserWidget* Widget = HearingGaugeWidgetComponent->GetUserWidgetObject();

		UE_LOG(LogGuardAI, Warning, TEXT("[%s] Hearing DrawSize = %s"), *GetName(), *HearingGaugeWidgetComponent->GetDrawSize().ToString());

		if (Widget)
		{
			UE_LOG(LogGuardAI, Warning, TEXT("[%s] Hearing DesiredSize = %s"), *GetName(), *Widget->GetDesiredSize().ToString());
		}
	}


}

void AGuardCharacter::SetHeadGaugeUpdateInterval(float NewInterval)
{
	HeadGaugeUpdateInterval = NewInterval;

	GetWorldTimerManager().ClearTimer(HeadGaugeUpdateTimerHandle);
	GetWorldTimerManager().SetTimer(HeadGaugeUpdateTimerHandle,
		this, &AGuardCharacter::UpdateHeadGaugeWidget, HeadGaugeUpdateInterval, true);
}

void AGuardCharacter::StopHeadGaugeUpdate()
{
	GetWorldTimerManager().ClearTimer(HeadGaugeUpdateTimerHandle);
}

void AGuardCharacter::UpdateHeadGaugeWidget()
{

	if (!IsValid(DetectionGaugeWidgetComponent))
	{
		return;
	}

	UDetectionGaugeWidget* GaugeWidget = Cast<UDetectionGaugeWidget>(DetectionGaugeWidgetComponent->GetUserWidgetObject());
	if (!GaugeWidget)
	{
		// Widget Class 가 아직 지정 안 됐거나(파생 BP에서 WBP_DetectionGauge 미설정),
		// 컴포넌트가 아직 위젯 인스턴스를 만들기 전(BeginPlay 타이밍)일 수 있다.
		return;
	}

	AGuardAIController* GuardAIController = Cast<AGuardAIController>(GetController());
	if (!GuardAIController)
	{
		GaugeWidget->SetGaugePercent(0.f);
		return;
	}

	// 인덱스 0 로컬 플레이어 기준. 이 프로토타입은 단일 플레이어 대상 테스트 씬이라
	// 화면 하나에 여러 로컬 플레이어가 동시에 있는 상황(스플릿스크린)은 다루지 않는다.
	const APawn* LocalPlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const float GaugePercent = GuardAIController->IsTargeting(LocalPlayerPawn) ? GuardAIController->GetDetectionGaugePercent() : 0.f;

	GaugeWidget->SetGaugePercent(GaugePercent);
}





void AGuardCharacter::SetGuardMoveSpeed(float NewMoveSpeed)
{
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->MaxWalkSpeed = NewMoveSpeed;
	}
}

FGenericTeamId AGuardCharacter::GetGenericTeamId() const
{
	const IGenericTeamAgentInterface* ControllerTeamAgent = Cast<IGenericTeamAgentInterface>(GetController());
	return ControllerTeamAgent ? ControllerTeamAgent->GetGenericTeamId() : FGenericTeamId::NoTeam;
}

bool AGuardCharacter::GetPatrolLocation(int32 Index, FVector& OutLocation) const
{
	if (PatrolPoints.Num() == 0)
	{
		return false;


	}

	const int32 SafeIndex = Index % PatrolPoints.Num();
	const AActor* Point = PatrolPoints[SafeIndex];

	if (!IsValid(Point))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] PatrolPoints[%d] 가 비어 있거나 파괴된 액터를 가리킨다."), *GetName(), SafeIndex);
		return false;
	}

	OutLocation = Point->GetActorLocation();
	return true;
}


