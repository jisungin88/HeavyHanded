#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoiseTestBlocker.generated.h"

class UStaticMeshComponent;

/**

 * TODO(출시 전): 이 UCLASS 는 쿠킹에 포함된다. 콘솔 명령과 달리 #if 로 못 빼므로
 *                에디터 전용 모듈로 옮기거나 실제 노획물 액터가 나오면 삭제할 것.
 *
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