#include "Character/Ability/AT_MonitorHeavyCarry.h"
#include "Character/BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UAT_MonitorHeavyCarry* UAT_MonitorHeavyCarry::MonitorHeavyCarry(
	UGameplayAbility* OwningAbility,
	ABaseCharacter* InAssistant,
	ABaseCharacter* InPrimaryCarrier,
	AActor* InCarriedItem,
	float InMaxAssistDistance,
	float InTugOpposingDotThreshold,
	float InTugTensionBuildRate,
	float InTugTensionDecayRate,
	float InTugTensionKnockbackThreshold)
{
	UAT_MonitorHeavyCarry* Task = NewAbilityTask<UAT_MonitorHeavyCarry>(OwningAbility);
	Task->Assistant = InAssistant;
	Task->PrimaryCarrier = InPrimaryCarrier;
	Task->CarriedItem = InCarriedItem;
	Task->MaxAssistDistance = InMaxAssistDistance;
	Task->TugOpposingDotThreshold = InTugOpposingDotThreshold;
	Task->TugTensionBuildRate = InTugTensionBuildRate;
	Task->TugTensionDecayRate = InTugTensionDecayRate;
	Task->TugTensionKnockbackThreshold = InTugTensionKnockbackThreshold;
	return Task;
}

void UAT_MonitorHeavyCarry::Activate()
{
	// bTickingTask = true 로 두면 이 태스크가 살아있는 동안 매 프레임 TickTask 가 호출된다.
	bTickingTask = true;
}

void UAT_MonitorHeavyCarry::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!Assistant.IsValid() || !PrimaryCarrier.IsValid() || !CarriedItem.IsValid())
	{
		EndWithReason(EHeavyCarryEndReason::PrimaryReleased);
		return;
	}

	// 주 운반자가 이미 물건을 놓았다면(SetHeldActor(nullptr) 등으로) 보조도 끝난다.
	if (PrimaryCarrier->GetHeldActor() != CarriedItem.Get())
	{
		EndWithReason(EHeavyCarryEndReason::PrimaryReleased);
		return;
	}

	const float DistSq = FVector::DistSquared(Assistant->GetActorLocation(), PrimaryCarrier->GetActorLocation());
	if (DistSq > FMath::Square(MaxAssistDistance))
	{
		EndWithReason(EHeavyCarryEndReason::DistanceExceeded);
		return;
	}

	UCharacterMovementComponent* AssistMove = Assistant->GetCharacterMovement();
	UCharacterMovementComponent* PrimaryMove = PrimaryCarrier->GetCharacterMovement();
	if (!AssistMove || !PrimaryMove)
	{
		return;
	}

	// 이동 여부가 아니라 "이동 입력"으로 판정한다 - 벽에 막혀 실제로는 못 움직여도
	// 반대로 당기려는 의도만으로 즉시 반응해야 스냅감이 생긴다.
	FVector AssistInput = AssistMove->GetLastInputVector();  AssistInput.Z = 0.f;
	FVector PrimaryInput = PrimaryMove->GetLastInputVector(); PrimaryInput.Z = 0.f;

	if (AssistInput.SizeSquared() < KINDA_SMALL_NUMBER || PrimaryInput.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		Tension = FMath::Max(0.f, Tension - TugTensionDecayRate * DeltaTime);
		return;
	}

	const float Dot = FVector::DotProduct(AssistInput.GetSafeNormal(), PrimaryInput.GetSafeNormal());

	if (Dot < TugOpposingDotThreshold)
	{
		Tension += TugTensionBuildRate * DeltaTime;
		if (Tension >= TugTensionKnockbackThreshold)
		{
			EndWithReason(EHeavyCarryEndReason::Knockback);
		}
	}
	else
	{
		Tension = FMath::Max(0.f, Tension - TugTensionDecayRate * DeltaTime);
	}
}

void UAT_MonitorHeavyCarry::EndWithReason(EHeavyCarryEndReason Reason)
{
	OnEnded.Broadcast(Reason);
	EndTask();
}
