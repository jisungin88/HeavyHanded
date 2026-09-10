#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PressurePlate.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USoundBase;
class ABaseCharacter;

/**
 * 일정 가치 이상의 노획물을 들고 지나가면 경보를 울리는 압력판.
 * (기획서 6장 Hazard.Sensor.PressurePlate 자리를 빌렸지만 판정 방식이 다르다 — 아래 참고)
 *
 * [기획서 원안과 다른 점 — 확인 필요]
 *   기획서 원안은 "위에 놓인 물건이 사라지면 반응"(무게 감지 — 물건을 빼돌리는 트릭)이다.
 *   이 클래스는 "비싼 물건을 들고 지나가면 반응"(금속탐지기 방식)으로 구현했다 — 사용자
 *   요청에 따른 재해석이다. Hazard.Sensor.PressurePlate 태그의 의미를 이걸로 굳힐지,
 *   원안대로 별도 구현이 필요한지는 오유석과 확인이 필요하다.
 *
 * [태그를 새로 안 만든 이유]
 *   Noise.ini 에 이 사건에 맞는 태그가 아직 없다. 새로 등록하려면 지성인의 Noise.ini 를
 *   건드려야 하는데, 지금은 그럴 필요가 없다 — 세계 경계도를 SetAlertGauge01() 로 직접
 *   올린다. 이 함수는 UAlertComponent 헤더에 "치트·스크립트 이벤트용" 이라고 명시돼 있어
 *   정확히 이런 용도로 이미 열려 있는 통로다.
 *
 * [그림자 이동은 무시한다]
 *   Config/Tags/State.ini 의 State.ShadowStep 코멘트에 "압력판 무시" 라고 이미 명시돼
 *   있다. HeavyHandedGameplayTags.h 에는 아직 이 태그가 선언돼 있지 않아 문자열 조회로
 *   참조한다(.ini 에는 이미 등록돼 있어 안전하다).
 *
 * [경보 다음에 놓친다]
 *   순서 자체에 필연적 이유는 없지만, "경보가 울렸다" 를 먼저 보여줘야 플레이어가
 *   무슨 일이 일어났는지 바로 이해한다.
 *
 * [경비는 안 걸린다]
 *   다른 Hazard 클래스들과 동일 원칙 — ABaseCharacter 로만 캐스트한다.
 *
 * 서버 권위 — 세계 경계도 변경·아이템 드롭 둘 다 서버 판정 사항이다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API APressurePlate : public AActor
{
	GENERATED_BODY()

public:
	APressurePlate();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 시각 전용. 콜리전 없음 — 바닥은 레벨의 실제 지오메트리가 담당한다 (ACreakyFloor 와 동일 사유) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	/** 밟았는지 판정하는 트리거 볼륨 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/** 들고 있는 노획물의 GetCurrentValue() 가 이 이상이어야 반응한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|PressurePlate",
		meta = (ClampMin = "0"))
	int32 ValueThreshold = 3000;

	/** 반응할 때 세계 경계도(0~1)에 더할 양 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|PressurePlate",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlertGaugeIncrease = 0.25f;

	/** 오버랩 경계에서 스치는 것만 걸러내는 디바운스 — 재무장 개념이 아니다 (ACreakyFloor 와 동일 사유) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|PressurePlate",
		meta = (ClampMin = "0.0", Units = "s"))
	float MinRetriggerInterval = 1.f;

	/** 경보음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<USoundBase> AlarmSound;

private:
	/** 경보가 울린 순간 모든 머신에서 재생한다. 상태를 남기지 않으므로 Unreliable 이다 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAlarmSound();
	void Multicast_PlayAlarmSound_Implementation();

	float LastTriggerTime = -1.f;
};
