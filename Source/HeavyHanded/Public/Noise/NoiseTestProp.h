#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoiseTestProp.generated.h"

class UNoiseEmitterComponent;
class UStaticMeshComponent;

/**

 * TODO(출시 전): 이 UCLASS 는 쿠킹에 포함된다. 콘솔 명령과 달리 #if 로 못 빼므로
 *                에디터 전용 모듈로 옮기거나 실제 노획물 액터가 나오면 삭제할 것.
 *
 * [디버그 전용] 물리 충돌 소음 테스트용 상자.
 * 실제 노획물 액터가 나오기 전까지 7단계를 검증하는 용도다.
 *
 * hh.Noise.SpawnProp 으로 플레이어 앞 상공에 떨어뜨린다.
 * 스폰 시 Instigator 를 플레이어 폰으로 심으므로 "최다 소음 유발자" 집계도 같이 돈다.
 *
 * 노획물 시스템이 들어오면 삭제할 것.
 */
UCLASS()
class HEAVYHANDED_API ANoiseTestProp : public AActor
{
	GENERATED_BODY()

public:
	ANoiseTestProp();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Noise|Debug")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Noise|Debug")
	TObjectPtr<UNoiseEmitterComponent> NoiseEmitter;
};
