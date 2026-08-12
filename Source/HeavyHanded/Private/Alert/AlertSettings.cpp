#include "Alert/AlertSettings.h"

#if WITH_EDITOR

#include "UObject/UnrealType.h"

void UAlertSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 이탈 임계값이 진입값 이상이면 히스테리시스가 사라져 단계가 깜빡인다
	SuspiciousExit = FMath::Min(SuspiciousExit, SuspiciousEnter);
	AlertedExit    = FMath::Min(AlertedExit,    AlertedEnter);

	// 경계 진입이 의심 진입보다 낮으면 단계 순서 자체가 뒤집힌다
	AlertedEnter   = FMath::Max(AlertedEnter,   SuspiciousEnter);
	AlertedExit    = FMath::Min(AlertedExit,    AlertedEnter);
}

#endif
