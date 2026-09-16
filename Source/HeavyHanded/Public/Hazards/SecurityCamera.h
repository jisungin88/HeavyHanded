#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SecurityCamera.generated.h"

class UStaticMeshComponent;
class USpotLightComponent;
class USoundBase;
class ABaseCharacter;

/**
 * 좌우로 시야를 훑다가 콘 안에 들어온 플레이어를 발견하면 경보를 울리는 감시 카메라.
 * (기획서 6장 — Hazard.Sensor.Camera, 박물관)
 *
 * [ALaserTrap / APressurePlate 와 같은 계열이지만 오버랩이 아니라 시야 판정이다]
 *   앞의 두 센서는 "지나가는 순간"을 오버랩 볼륨으로 잡지만, 카메라는 매 순간 "지금
 *   콘 안에 있는가" 를 판정해야 해서 오버랩이 아니라 주기적인 각도·거리·시야 트레이스로
 *   확인한다. Hazards 클래스 중 Tick 을 쓰는 것은 이 클래스가 유일하다.
 *
 * [회전은 서버 시간으로 계산한다 — 복제하지 않는다]
 *   회전 각도를 매 틱 복제하면 대역폭 낭비고, 그렇다고 각 머신이 자기 GetTimeSeconds() 로
 *   계산하면 접속 시점 차이 때문에 서버·클라이언트가 살짝 어긋난 위상을 보게 된다 —
 *   "화면에서는 피한 것처럼 보이는데 서버는 걸렸다고 판정" 하는 불공평이 생길 수 있다.
 *   AGameStateBase::GetServerWorldTimeSeconds() 는 그 접속 오차를 보정해 주는 엔진
 *   표준 API라, 모든 머신이 같은 사인파 위상을 계산하게 되어 회전 트랜스폼을 복제할
 *   필요 자체가 없어진다.
 *
 * [CameraBase 를 통째로 돌린다 — CameraHead 만 도는 게 아니다]
 *   렌즈(CameraHead)만 몸체(CameraBase) 위에서 따로 도는 것보다, 카메라 전체가 한
 *   덩어리로 팬(pan)하는 쪽이 실제 감시 카메라처럼 자연스럽다는 판단에 따라 루트인
 *   CameraBase 에 스윕 회전을 준다. CameraHead 는 CameraBase 에 고정 부착만 돼 있을 뿐
 *   따로 애니메이션되지 않는다 — 부모가 돌면 자식도 같은 월드 회전을 그대로 물려받으므로
 *   감지 판정(CameraHead 의 Forward/위치를 매번 새로 읽는 CheckDetection)은 손댈 필요가 없다.
 *
 * [경비는 안 걸린다 / 그림자 이동 무시 — 다른 센서와 동일 원칙]
 *   OtherActor 를 ABaseCharacter 로만 캐스트하고, State.ShadowStep 태그가 있으면
 *   무시한다 (ALaserTrap::OnTriggerOverlap 과 동일 사유).
 *
 * [Disable() — EMP 연동을 위해 미리 열어 둔 진입점]
 *   장비 쪽(EMP, 기획서 7장 — $10,000, "카메라 15초 무력화")이 이 기능을 구현할 때
 *   이 함수 하나만 부르면 된다. 지금은 호출하는 쪽(Equipment)이 없다 — 실제로 연결할
 *   때는 물리·아이템 담당과 상의해서 진행할 것.
 *
 * [VisionCone — 판정값과 어긋나지 않는 시야 표시]
 *   플레이어가 위험 구역을 눈으로 보고 피할 수 있어야 한다는 요청에 따라 스포트라이트로
 *   시야를 그대로 보여준다. 별도 수치를 BP 에 다시 입력하면 언젠가 DetectionRange/
 *   DetectionHalfAngleDegrees 와 어긋난다(CLAUDE.md 3절 "수치 하드코딩 금지") — 그래서
 *   OnConstruction 에서 이 두 값으로 스포트라이트 각도·반경을 강제로 맞춘다. 색상·밝기처럼
 *   판정과 무관한 값만 BP 에서 자유롭게 조정하면 된다.
 *
 * 서버 권위 — 판정(감지·경계도 변경)은 서버에서만 한다. 회전 연출은 각 머신이 각자 계산한다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API ASecurityCamera : public AActor
{
	GENERATED_BODY()

public:
	ASecurityCamera();

	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Duration 초 동안 감지를 멈춘다. EMP 같은 장비가 이 카메라를 무력화할 때 호출하는
	 * 진입점 — 아직 호출하는 쪽은 없다 (헤더 주석 참고).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hazard|Camera")
	void Disable(float Duration);

	UFUNCTION(BlueprintPure, Category = "Hazard|Camera")
	bool IsDisabled() const { return bDisabled; }

protected:
	virtual void BeginPlay() override;

	/** 카메라 몸체. 루트이며 좌우로 스윕하는 회전의 실제 주체다(CameraHead 는 여기 고정 부착돼 같이 돈다) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> CameraBase;

	/**
	 * 렌즈 부분. CameraBase 에 고정 부착돼 있고 따로 애니메이션되지 않는다 — 이 컴포넌트의
	 * 정면(Forward)이 감지 콘의 중심축이라는 점만 그대로 유지된다(CameraBase 가 돌면 같이 돈다).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> CameraHead;

	/**
	 * 실제 감지 판정을 그대로 시각화하는 스포트라이트. 각도·반경은 코드가 자동으로
	 * DetectionRange/DetectionHalfAngleDegrees 에 맞춘다 — 색상·밝기만 BP 에서 조정할 것.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<USpotLightComponent> VisionCone;

	/** 감지 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Camera",
		meta = (ClampMin = "0.0", Units = "cm"))
	float DetectionRange = 1500.f;

	/** 콘의 절반각(도). 정면 기준 좌우로 이 각도 안에 있어야 걸린다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Camera",
		meta = (ClampMin = "1.0", ClampMax = "90.0"))
	float DetectionHalfAngleDegrees = 25.f;

	/** 렌즈가 기준 방향에서 좌우로 훑는 최대 각도(도) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Camera",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SweepAngleDegrees = 60.f;

	/** 왕복 한 번(좌→우→좌)에 걸리는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Camera",
		meta = (ClampMin = "0.1", Units = "s"))
	float SweepPeriod = 6.f;

	/** 감지 판정을 다시 확인하는 주기. 매 틱 트레이스를 쏘지 않으려고 둔 간격이다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Camera",
		meta = (ClampMin = "0.05", Units = "s"))
	float DetectionCheckInterval = 0.2f;

	/** 반응할 때 세계 경계도(0~1)에 더할 양 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Camera",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlertGaugeIncrease = 0.25f;

	/** 한 번 걸린 뒤 다시 걸릴 때까지의 최소 간격 — 계속 시야 안에 있어도 매번 울리지 않게 한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Camera",
		meta = (ClampMin = "0.0", Units = "s"))
	float MinRetriggerInterval = 3.f;

	/**
	 * 발견하는 순간 호출된다. 판정(경보·경계도)은 이미 끝난 뒤이므로 여기서 게임 상태를
	 * 더 바꾸지 않는다 — 경고등이 빨갛게 바뀌는 등 연출은 BP 에서 이 이벤트에 붙인다
	 * (ALaserTrap::OnLaserTriggered 와 같은 역할의 훅).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard|Camera")
	void OnPlayerDetected();

	/** 경보음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<USoundBase> AlarmSound;

private:
	/** 현재 서버 시각 기준으로 CameraBase 가 있어야 할 요(Yaw) 오프셋을 계산한다. 모든 머신에서 같은 값이 나온다 */
	float ComputeSweepYawOffset() const;

	/** 서버 전용 감지 판정. DetectionCheckInterval 마다 불린다 */
	void CheckDetection();

	/** VisionCone 의 각도·반경을 DetectionRange/DetectionHalfAngleDegrees 로 맞춘다 (헤더 주석 참고) */
	void SyncVisionConeToDetectionParams();

	/** Disable() 이 건 타이머가 Duration 뒤 호출해 bDisabled 를 다시 끈다 */
	void ReEnable();

	/** 경보가 울린 순간 모든 머신에서 재생한다. 상태를 남기지 않으므로 Unreliable 이다 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAlarmSound();
	void Multicast_PlayAlarmSound_Implementation();

	/** CameraBase 의 초기 상대 회전 — 스윕 오프셋은 이 값에 더해진다 */
	FRotator BaseBodyRotation;

	/**
	 * true 면 감지를 멈추고 렌즈도 정면에 고정된다(Tick 에서 읽는다). EMP 등으로
	 * 무력화된 동안 값이 유지돼야 하는 "상태"라 CLAUDE.md 3절 규칙대로 복제 프로퍼티로
	 * 둔다 — Multicast 로 흉내 내면 늦게 접속한 클라가 무력화 상태를 못 본다.
	 */
	UPROPERTY(Replicated)
	bool bDisabled = false;

	float LastTriggerTime = -1.f;

	FTimerHandle DetectionTimer;
	FTimerHandle DisableTimer;
};
