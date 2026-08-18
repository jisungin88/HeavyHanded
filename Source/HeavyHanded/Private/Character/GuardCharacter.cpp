#include "Character/GuardCharacter.h"
#include "AI/GuardTypes.h"

#include "Noise/PerceptionMeterComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"

AGuardCharacter::AGuardCharacter()
{
	PerceptionMeter = CreateDefaultSubobject<UPerceptionMeterComponent>(TEXT("PerceptionMeter"));

	// Screen space로 두면 항상 카메라를 향해 평면으로 그려지므로(빌보드), World space처럼
	// 경비가 돌아설 때 게이지가 옆으로 눕는 문제가 없다. 위치만 머리 위로 올려서 붙인다.
	DetectionGaugeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DetectionGaugeWidgetComponent"));
	DetectionGaugeWidgetComponent->SetupAttachment(GetCapsuleComponent());
	DetectionGaugeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DetectionGaugeWidgetComponent->SetDrawSize(FVector2D(120.f, 16.f));
	DetectionGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	// 위젯 클래스는 여기서 강제하지 않는다 - BP_GuardBase 등 파생 BP에서
	// 컴포넌트 디테일 패널의 Widget Class로 WBP_DetectionGauge를 지정할 것.
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
