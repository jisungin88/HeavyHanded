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
 * 경비 1명의 "소리를 들었다" 게이지.
 * 소음 자극을 누적하고, 무자극이 이어지면 감소한다. 100% 에서 래치되며
 * ResetPerception() 만이 래치를 푼다.
 *
 * 누적 계산은 여기(김지성), 100% 이후의 조사 행동과 BT 는 유정석.
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
    void OnRep_Perception();

    /** 0~1. 서버가 계산하고, 머리 위 게이지 위젯을 위해 복제된다 */
    UPROPERTY(ReplicatedUsing = OnRep_Perception, BlueprintReadOnly, Category = "Noise|Perception")
    float Perception01 = 0.f;

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

    /** 귀 높이. Owner 가 폰이 아닐 때만 쓰는 오프셋 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Perception", meta = (Units = "cm"))
    float EarHeight = 60.f;

private:
    /** UActorComponent 에는 HasAuthority() 가 없다. Owner 의 롤로 판단한다 */
    bool HasNoiseAuthority() const;

    /** 클램프 · RepNotify · 래치 · 틱 on/off 를 한곳에서 처리한다 */
    void SetPerception(float NewValue);

    FVector LastNoiseLocation = FVector::ZeroVector;

    float TimeSinceLastStimulus = 0.f;

    /** 100% 도달 후 재발화 방지. ResetPerception() 만이 푼다 */
    bool bLatched = false;
};