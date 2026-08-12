#include "Character/GuardCharacter.h"

#include "Noise/PerceptionMeterComponent.h"

AGuardCharacter::AGuardCharacter()
{
	PerceptionMeter = CreateDefaultSubobject<UPerceptionMeterComponent>(TEXT("PerceptionMeter"));
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
		return false;
	}

	OutLocation = Point->GetActorLocation();
	return true;
}
