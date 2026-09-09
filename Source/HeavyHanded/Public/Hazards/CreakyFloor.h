#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CreakyFloor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;
class ABaseCharacter;

/**
 * 밟으면 소리가 나서 주변 경비가 감지하는 바닥. (기획서 6장 — Hazard.Sensor.CreakyFloor, 저택)
 *
 * [AMovementTrap/APuddle 과 같은 구조 — 트리거 볼륨이 판정, 메시는 시각 전용]
 *   FloorMesh 는 콜리전이 없다. 이 액터는 기존 바닥 위에 겹쳐 놓는 장식+트리거 조합이지
 *   바닥 자체를 대체하지 않는다 — 걷는 데 필요한 콜리전은 레벨의 실제 바닥이 담당한다.
 *
 * [게임플레이 소음과 SFX 를 분리한다 — ALootBase::OnImpact 와 같은 원칙]
 *   ReportNoise 호출(경비가 반응하는 축)과 CreakSound 재생(플레이어가 듣는 축)은
 *   서로 다른 채널이다. 나중에 "소리만 나고 경비는 못 듣는다" 나 그 반대로 튜닝해도
 *   서로 안 엮인다.
 *
 * [MinRetriggerInterval — 곰덫과 다르게 재무장 개념이 없다]
 *   삐걱대는 마루는 기계 장치가 아니라 밟을 때마다 우는 것이 자연스럽다. 그래서
 *   AMovementTrap::ResetDelay 같은 "쉬는 시간" 대신, 오버랩 경계에서 잠깐 들어왔다
 *   나갔다 하는 것(스침)만 걸러내는 짧은 디바운스만 둔다.
 *
 * 서버 권위. 소음 발행은 서버에서만 의미가 있다 — 판정에 복제할 상태가 없다
 * (걸렸는지/안 걸렸는지가 남지 않는다. AVaultDoor/ABreakableWall 과 다른 점).
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API ACreakyFloor : public AActor
{
	GENERATED_BODY()

public:
	ACreakyFloor();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 시각 전용. 콜리전 없음 — 걷는 판정은 레벨의 실제 바닥이 한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	/** 밟았는지 판정하는 트리거 볼륨. 바닥 발판 범위에 맞춰 BP 에서 조정할 것 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/**
	 * 같은 대상이 다시 소리를 내는 데 필요한 최소 간격. 재무장 개념이 아니라
	 * 오버랩 경계에서 미세하게 들락거릴 때(스침) 소음이 연속으로 터지는 것만 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Creak",
		meta = (ClampMin = "0.0", Units = "s"))
	float MinRetriggerInterval = 0.5f;

	/** 밟았을 때 플레이어가 듣는 삐걱거리는 소리. 경비 감지(ReportNoise)와는 별개 채널이다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<USoundBase> CreakSound;

private:
	/** 소리를 낸 순간 모든 머신에서 재생한다. 상태를 남기지 않으므로 Unreliable 이다 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayCreakSound();
	void Multicast_PlayCreakSound_Implementation();

	/** 마지막으로 소리를 낸 시각(GetWorld()->GetTimeSeconds() 기준). 복제하지 않는다 — 서버 전용 판정 */
	float LastTriggerTime = -1.f;
};
