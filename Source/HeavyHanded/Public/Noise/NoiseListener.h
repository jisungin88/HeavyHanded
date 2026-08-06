#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Noise/NoiseTypes.h"
#include "NoiseListener.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UNoiseListener : public UInterface
{
	GENERATED_BODY()
};

/**
 * 소음을 듣는 쪽. 경비의 PerceptionMeterComponent가 주 구현체이고,
 * 감지 장치나 미믹처럼 소음에 반응하는 것이면 무엇이든 붙일 수 있다.
 * 등록/해제는 구현체가 직접 한다 (BeginPlay / EndPlay).
 */
class INoiseListener
{
	GENERATED_BODY()

public:
	/** 감쇄까지 끝난 자극이 전달된다. 서버에서만 호출된다 */
	UFUNCTION(BlueprintNativeEvent, Category = "Noise")
	void OnNoiseHeard(const FNoiseStimulus& Stimulus);

	/** 청취 지점(귀 위치). 오클루전 트레이스의 끝점이 된다 */
	UFUNCTION(BlueprintNativeEvent, Category = "Noise")
	FVector GetListenerLocation() const;
};

//BlueprintNativeEvent로 둔 이유 경비 BP에서 직접 오버라이드할 수 있게 하기 위해서
//C++에서 호출할 때는 INoiseListener::Execute_OnNoiseHeard(Object, Stimulus)