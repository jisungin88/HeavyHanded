#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoiseTestBlocker.generated.h"

class UStaticMeshComponent;

/**
 * [디버그 전용] 소음 차폐 테스트용 벽.
 * NoiseOcclusion 채널의 기본 응답이 Ignore 라서 일반 큐브는 소음을 못 막는다.
 * 이 액터는 그 채널만 Block 으로 켜 둔 큐브다. 스케일을 늘려서 벽으로 쓴다.
 */
UCLASS()
class HEAVYHANDED_API ANoiseTestBlocker : public AActor
{
	GENERATED_BODY()

public:
	ANoiseTestBlocker();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Noise|Debug")
	TObjectPtr<UStaticMeshComponent> Body;
};