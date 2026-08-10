#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoiseTestListener.generated.h"

class UPerceptionMeterComponent;
class UStaticMeshComponent;

/**

 * TODO(출시 전): 이 UCLASS 는 쿠킹에 포함된다. 콘솔 명령과 달리 #if 로 못 빼므로
 *                에디터 전용 모듈로 옮기거나 실제 노획물 액터가 나오면 삭제할 것.
 *
 * [디버그 전용] 소음을 듣는 더미. 경비 액터가 나오기 전까지 4~5단계를 검증하는 용도다.
 * 머리 위에 인지 게이지를 실시간으로 그린다. 서버/클라 창을 구분해서 표시하므로
 * 복제가 되는지도 눈으로 확인된다.
 *
 * 경비 AI 가 들어오면 삭제할 것.
 */
UCLASS()
class HEAVYHANDED_API ANoiseTestListener : public AActor
{
	GENERATED_BODY()

public:
	ANoiseTestListener();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Noise|Debug")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Noise|Debug")
	TObjectPtr<UPerceptionMeterComponent> PerceptionMeter;

	UFUNCTION()
	void HandlePerceptionFull(FVector LastNoiseLocation);
};