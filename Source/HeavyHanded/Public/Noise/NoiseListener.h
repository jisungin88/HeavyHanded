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
 * 소음을 듣는 쪽. 경비의 PerceptionMeterComponent 가 주 구현체다.
 * 등록/해제는 구현체가 직접 한다 (BeginPlay / EndPlay).
 */
class INoiseListener
{
	GENERATED_BODY()

public:
	/** 감쇄까지 끝난 자극이 전달된다. 서버에서만 호출된다 */
	UFUNCTION(BlueprintNativeEvent, Category = "Noise")
	void OnNoiseHeard(const FNoiseStimulus& Stimulus);

	/**
	 * 청취 지점(귀 위치). 오클루전 트레이스의 끝점이 된다.
	 * **계약 — 소유 액터 위치에서 ListenerCullMargin(500cm) 안이어야 한다.** 서브시스템이
	 * 액터 위치로 1차 컬링을 한 뒤에 부르므로, 벗어나면 반경 경계의 청취자가 조용히 걸러진다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Noise")
	FVector GetListenerLocation() const;
};

//BlueprintNativeEvent로 둔 이유 경비 BP에서 직접 오버라이드할 수 있게 하기 위해서
//C++에서 호출할 때는 INoiseListener::Execute_OnNoiseHeard(Object, Stimulus)