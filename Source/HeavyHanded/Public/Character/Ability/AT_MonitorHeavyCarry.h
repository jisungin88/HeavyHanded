#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AT_MonitorHeavyCarry.generated.h"

class ABaseCharacter;

UENUM(BlueprintType)
enum class EHeavyCarryEndReason : uint8
{
	DistanceExceeded,
	Knockback,
	PrimaryReleased
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHeavyCarryEndedDelegate, EHeavyCarryEndReason, Reason);

// 무거운 물건을 둘이 드는 동안 거리·줄다리기 텐션을 매 프레임 감시하는 태스크.
// 이 태스크가 살아있다는 것 자체가 "지금 보조 중"이라는 상태다 -
// 별도 bool 플래그를 캐릭터에 둘 필요가 없다.
UCLASS()
class HEAVYHANDED_API UAT_MonitorHeavyCarry : public UAbilityTask
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FHeavyCarryEndedDelegate OnEnded;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAT_MonitorHeavyCarry* MonitorHeavyCarry(
		UGameplayAbility* OwningAbility,
		ABaseCharacter* InAssistant,
		ABaseCharacter* InPrimaryCarrier,
		AActor* InCarriedItem,
		float InMaxAssistDistance,
		float InTugOpposingDotThreshold,
		float InTugTensionBuildRate,
		float InTugTensionDecayRate,
		float InTugTensionKnockbackThreshold);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	// GA_HeavyCarryAssist 가 Event.Loot.Dropped 를 즉시 받았을 때(폴링 대기 없이)
	// 밖에서 조기 종료시킬 수 있도록 공개한다.
	void EndWithReason(EHeavyCarryEndReason Reason);

private:
	TWeakObjectPtr<ABaseCharacter> Assistant;
	TWeakObjectPtr<ABaseCharacter> PrimaryCarrier;
	TWeakObjectPtr<AActor> CarriedItem;

	float MaxAssistDistance = 250.f;
	float TugOpposingDotThreshold = -0.5f;
	float TugTensionBuildRate = 1.f;
	float TugTensionDecayRate = 2.f;
	float TugTensionKnockbackThreshold = 1.f;

	// 이 값의 생애주기 = 태스크(=보조)의 생애주기. 딱 맞는 스코프라 별도 초기화가 필요 없다.
	float Tension = 0.f;
};
