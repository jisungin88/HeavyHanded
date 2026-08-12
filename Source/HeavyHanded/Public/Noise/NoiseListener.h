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

	/**
	 * 청취 지점(귀 위치). 오클루전 트레이스의 끝점이 된다.
	 *
	 * 계약: 반환 지점은 소유 액터 위치에서 500cm(UNoiseSubsystem 의 ListenerCullMargin) 안이어야 한다.
	 * UNoiseSubsystem 이 액터 위치로 값싼 1차 거리 컬링을 한 뒤에야 이 함수를 부르기 때문이다
	 * (BlueprintNativeEvent 라 호출 자체가 ProcessEvent 를 탄다).
	 *
	 * 이 범위를 벗어나면 반경 경계의 청취자가 조용히 걸러진다.
	 * 개발 빌드에서는 ensure 로 잡히지만 쉬핑에서는 아무 신호도 없다.
	 * 액터에서 멀리 떨어진 지점을 듣게 하려면 별도 청취자를 그 자리에 두는 편이 맞다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Noise")
	FVector GetListenerLocation() const;
};

//BlueprintNativeEvent로 둔 이유 경비 BP에서 직접 오버라이드할 수 있게 하기 위해서
//C++에서 호출할 때는 INoiseListener::Execute_OnNoiseHeard(Object, Stimulus)