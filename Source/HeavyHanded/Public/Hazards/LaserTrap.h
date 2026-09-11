#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LaserTrap.generated.h"

class UNiagaraComponent;
class UBoxComponent;
class USoundBase;
class ABaseCharacter;

/**
 * 레이저를 가로지르면 경보를 울리고 세계 경계도를 올리는 감지 장치.
 * (기획서 6장 — Hazard.Sensor.LaserGrid, 박물관)
 *
 * [APressurePlate 와 거의 같은 패턴]
 *   "무엇을 트리거로 보는가" 만 다르다 — 압력판은 "비싼 물건을 들고 지나감",
 *   레이저는 "누구든 그냥 지나감". 그래서 노획물 가치 체크·아이템 드롭 로직이 없다.
 *   경보·세계 경계도 처리는 완전히 같은 방식(SetAlertGauge01 직접 호출)이다.
 *
 * [태그를 새로 안 만든 이유]
 *   APressurePlate 와 동일 — Noise.ini 에 맞는 태그가 없어서 새로 등록하는 대신
 *   세계 경계도를 SetAlertGauge01() 로 직접 올린다("치트 · 스크립트 이벤트용" 으로
 *   AlertComponent.h 에 이미 열려 있는 통로).
 *
 * [BeamEffect 는 상시 재생 — 다른 Hazard 의 "트리거 순간 연출"과 다르다]
 *   덫/웅덩이/압력판의 이펙트는 "걸리는 순간"에만 스폰되지만, 레이저 빔은 항상 보여야
 *   하는 시각 요소다. 그래서 컴포넌트로 붙여 두고 BP 가 Niagara System 에셋만 지정하면
 *   BeginPlay 에 자동 재생된다(UNiagaraComponent 기본 동작, bAutoActivate).
 *
 * [경비는 안 걸린다]
 *   다른 Hazard 클래스들과 동일 원칙 — ABaseCharacter 로만 캐스트한다. 경비가 순찰하는
 *   구역에는 레이저를 배치하지 않는다는 전제다(레이저 통과 자체를 막을 방법은 없다).
 *
 * 서버 권위 — 세계 경계도 변경은 서버 판정 사항이다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API ALaserTrap : public AActor
{
	GENERATED_BODY()

public:
	ALaserTrap();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 상시 재생되는 레이저 빔 시각 효과. 루트다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UNiagaraComponent> BeamEffect;

	/**
	 * 레이저를 가로지르는지 판정하는 볼륨. 기본값은 얇고 넓게 잡아뒀다 —
	 * 실제 통로 폭·높이에 맞춰 BP 에서 Box Extent 를 조정할 것.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/** 반응할 때 세계 경계도(0~1)에 더할 양 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|LaserTrap",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlertGaugeIncrease = 0.25f;

	/** 오버랩 경계에서 스치는 것만 걸러내는 디바운스 — 재무장 개념이 아니다 (다른 Hazard 클래스와 동일 사유) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|LaserTrap",
		meta = (ClampMin = "0.0", Units = "s"))
	float MinRetriggerInterval = 1.f;

	/**
	 * 레이저에 걸렸을 때 호출된다. 판정(경보·경계도)은 이미 끝난 뒤이므로 여기서 게임
	 * 상태를 더 바꾸지 않는다 — 문을 닫는 등 레벨별 연출/연결은 BP 에서 이 이벤트에 붙인다.
	 *
	 * [서버 전용이다 — 이 함수를 부르는 OnTriggerOverlap 자체가 HasAuthority() 로 막혀 있다]
	 *   여기서 문을 닫기로 했다면, 그 문 BP 자신이 복제(Replicated bool + RepNotify)로
	 *   클라이언트 전파를 책임져야 한다 — AVaultDoor/ABreakableWall 과 같은 원칙이다.
	 *   이 이벤트 자체를 클라에서도 실행시키면 서버·클라가 각자 다르게 판단할 위험이 생긴다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard|LaserTrap")
	void OnLaserTriggered();

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
