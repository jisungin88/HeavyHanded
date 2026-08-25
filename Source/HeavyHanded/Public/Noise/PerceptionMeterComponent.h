#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Noise/NoiseListener.h"
#include "Noise/NoiseTypes.h"
#include "PerceptionMeterComponent.generated.h"   // 반드시 마지막

/** 인지 게이지 100% 도달. 서버에서만 발생하며 유정석의 BT 가 구독한다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerceptionFull, FVector, LastNoiseLocation);

/** 게이지 값 변화. 머리 위 게이지 위젯용. 서버·클라 양쪽에서 발생한다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerceptionChanged, float, NewPerception01);

/**
 * 경비 1명의 "소리를 들었다" 게이지. 자극을 누적하고 무자극이 이어지면 감소한다.
 * 100% 에서 래치되며 ResetPerception() 만이 푼다. 그 뒤의 조사 행동과 BT 는 유정석.
 */
UCLASS(ClassGroup = (Noise), meta = (BlueprintSpawnableComponent))
class HEAVYHANDED_API UPerceptionMeterComponent : public UActorComponent, public INoiseListener
{
	GENERATED_BODY()

public:
	UPerceptionMeterComponent();

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End

	//~ INoiseListener — BlueprintNativeEvent 라 _Implementation 을 오버라이드한다
	virtual void OnNoiseHeard_Implementation(const FNoiseStimulus& Stimulus) override;
	virtual FVector GetListenerLocation_Implementation() const override;
	//~ End

	/** 조사 종료 후 반드시 호출할 것. 이게 없으면 경비가 영원히 100% 에 박혀 있다 */
	UFUNCTION(BlueprintCallable, Category = "Noise|Perception")
	void ResetPerception();

	UFUNCTION(BlueprintPure, Category = "Noise|Perception")
	float GetPerception01() const { return Perception01; }

	UFUNCTION(BlueprintPure, Category = "Noise|Perception")
	bool IsLatched() const { return bLatched; }

	/** 마지막으로 들은 소음 지점. 조사 목적지로 쓴다 (서버 권위) */
	UFUNCTION(BlueprintPure, Category = "Noise|Perception")
	FVector GetLastNoiseLocation() const { return LastNoiseLocation; }

	UPROPERTY(BlueprintAssignable, Category = "Noise|Perception")
	FOnPerceptionFull OnPerceptionFull;

	UPROPERTY(BlueprintAssignable, Category = "Noise|Perception")
	FOnPerceptionChanged OnPerceptionChanged;

protected:
	UFUNCTION()
	void OnRep_ReplicatedPerception();

	/**
	 * 0~1. 서버 권위. 이 값 자체는 복제하지 않는다 — ReplicatedPerception 이 대신한다.
	 * 서버에서는 정밀한 실수값, 클라에서는 양자화를 되돌린 값이 들어 있다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Noise|Perception")
	float Perception01 = 0.f;

	/**
	 * Perception01 을 0~255 로 양자화한 복제본. 머리 위 게이지 위젯용.
	 * float 를 그대로 복제하면 감소 중 매 프레임 dirty 가 되어 경비 수만큼 곱해진다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedPerception)
	uint8 ReplicatedPerception = 0;

	/** Strength 1.0 짜리 자극 1건이 올리는 양 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Perception",
					  meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GainPerStimulus = 0.35f;

	/** 무자극 유예. 이 시간이 지나야 감소가 시작된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Perception",
					  meta = (ClampMin = "0.0", Units = "s"))
	float DecayGraceSeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Perception", meta = (ClampMin = "0.0"))
	float DecayPerSecond = 0.2f;

	/**
	 * 귀 높이. Owner 가 폰이 아닐 때만 쓰는 오프셋.
	 * ClampMax 는 UNoiseSubsystem 의 ListenerCullMargin 과 묶여 있다 — 그보다 크게 열면
	 * 반경 경계의 청취자가 1차 거리 컬링에서 조용히 걸러진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Perception",
			  meta = (ClampMin = "0.0", ClampMax = "300.0", Units = "cm"))
	float EarHeight = 60.f;

private:
	// 권위 판정은 Shared/NetAuthority.h 의 HasServerAuthority(this) 하나로 통일했다.
	// UActorComponent 에는 HasAuthority() 가 없어서 컴포넌트마다 각자 만들던 것을 모았다

	/** 클램프 · RepNotify · 래치 · 틱 on/off 를 한곳에서 처리한다 */
	void SetPerception(float NewValue);

	FVector LastNoiseLocation = FVector::ZeroVector;

	float TimeSinceLastStimulus = 0.f;

	/** 100% 도달 후 재발화 방지. ResetPerception() 만이 푼다 */
	bool bLatched = false;
};