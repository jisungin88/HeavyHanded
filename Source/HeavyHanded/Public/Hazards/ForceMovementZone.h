#pragma once

#include "CoreMinimal.h"
#include "Hazards/HazardBase.h"
#include "ForceMovementZone.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class ABaseCharacter;
class ALootBase;
class USoundBase;
class UAudioComponent;

/**
 * 안에 있는 동안 정해진 방향으로 강제 이동시키는 장치. (기획서 6장 — Hazard.Transport.*)
 * 수하물 컨베이어(박물관) / 문서 이송 튜브(은행) — 미는 로직은 완전히 같고, 메시·속도·
 * bOverrideControl 값만 다르다. 기획서가 말한 "동일 베이스 클래스 + 장소별 메시·파라미터"가
 * 정확히 들어맞는 첫 케이스라 여기서는 클래스를 나누지 않고 BP 2개로 간다.
 *
 * [컨베이어와 튜브는 "저항 가능 여부"가 다르다]
 *   컨베이어는 걸어가는 벨트 위라 반대로 걸으면 어느 정도 버틸 수 있어야 자연스럽고,
 *   튜브는 사람이 낄 정도로 좁은 이송관이라 한 번 빨려 들어가면 저항할 방법이 없어야
 *   자연스럽다. bOverrideControl 하나로 이 차이를 표현한다 — true 면 DisableMovement() 로
 *   본인 조작 자체를 끊어 버리고 이 클래스의 강제 이동만 남는다.
 *
 * [Tick 을 쓰는 유일한 Hazard 클래스]
 *   덫/웅덩이/마루는 전부 BeginOverlap/EndOverlap 이벤트만으로 충분했다. 강제 이동은
 *   "그 안에 있는 동안 계속" 밀어야 하는 지속 효과라 매 프레임 갱신이 필요하다.
 *
 * [AddActorWorldOffset(스윕 켜짐)을 쓰는 이유]
 *   AddMovementInput 을 쓰면 실제로 얼마나 밀리는지가 캐릭터 자신의 가속도·최대속도에
 *   묶여서 ForceSpeed 값이 정확한 의미를 가지지 못한다. 장소별로 "속도만 다르게" 튜닝
 *   하려면 컨베이어 스스로 정확한 cm/s 를 갖고 있어야 해서, 캐릭터를 직접 그만큼 밀어낸다.
 *   스윕을 켜는 이유는 밀다가 벽을 뚫지 않게 하기 위해서다.
 *
 *   [점프로 빠져나가는 문제 — 속도 성분 상쇄로 해결]
 *   AddActorWorldOffset 는 캐릭터 자신의 CharacterMovementComponent 와 별도 경로라,
 *   자기 이동 속도(특히 점프 중 에어 컨트롤)가 ForceSpeed 보다 빠르면 단순히 더하는
 *   구조상 상쇄되지 않고 반대 방향으로 뚫고 나갈 수 있었다. 그래서 Tick 에서 매 프레임
 *   Velocity 중 벨트를 거스르는 성분만 0으로 되돌린 뒤 Offset 을 더한다 — 걷기든
 *   점프든 반대 방향으로는 못 가고, 옆/정방향 이동은 그대로 자유롭다.
 *
 * [경비는 안 걸린다]
 *   OtherActor 를 ABaseCharacter 로만 캐스트해 Occupants 에 담는다. 다른 Hazard
 *   클래스들과 같은 이유로, 경비가 순찰 중 컨베이어에 실려 엉뚱한 곳으로 안 간다.
 *
 * [노획물도 실려 간다 — 단, 민 방식은 캐릭터와 다르다]
 *   ABaseCharacter 로 캐스트가 안 되면 ALootBase 인지 한 번 더 확인해 LootOccupants 에
 *   담는다. 노획물은 CharacterMovementComponent 가 없고 물리 바디(GetPhysicsRoot())로
 *   움직이므로, AddActorWorldOffset 로 순간이동시키면 물리가 떨리거나 깨진다 —
 *   대신 벨트 축 방향 속도만 SetPhysicsLinearVelocity 로 맞춰서 물리 시뮬레이션이
 *   계속 자연스럽게 돌아가게 한다. 누가 들고 있는 중(GetPrimaryCarrier() != nullptr)
 *   이면 플레이어 이동에 이미 딸려가므로 밀지 않는다.
 *
 * 서버 권위 — 미는 판정은 서버에서만 한다. 결과(캐릭터 위치)는 이동 복제로 전파된다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API AForceMovementZone : public AHazardBase
{
	GENERATED_BODY()

public:
	AForceMovementZone();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/**
	 * 컨베이어 벨트 / 튜브 바닥 메시. 콜리전을 그대로 둔다 — 발판이라 실제로 밟고 서야 한다
	 * (다른 Hazard 클래스의 장식 전용 메시와 다른 점).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> ZoneMesh;

	/** 강제 이동 판정 볼륨. 장치 발판 범위에 맞춰 BP 에서 조정할 것 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> ForceVolume;

	// 단위: cm/s (UE 에는 cm/s 단위 지정자가 없어 Units 메타 없이 클램프만 건다 — GuardTypes.h 와 동일 사유)
	/** 밀어내는 속도. 액터의 정면(+X)이 이동 방향이다 — 배치할 때 그쪽으로 회전시킬 것 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Transport",
		meta = (ClampMin = "0.0"))
	float ForceSpeed = 300.f;

	/**
	 * true 면 안에 있는 동안 본인 조작(WASD 등)을 완전히 끊는다 — 튜브처럼 저항이
	 * 불가능해야 하는 장치에 켠다. false(기본, 컨베이어)면 본인 조작은 그대로 살아있고
	 * 이 클래스의 강제 이동만 얹힌다(반대로 걸으면 어느 정도 버틸 수 있다).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Transport")
	bool bOverrideControl = false;

	/**
	 * 작동 중(발판 위에 누군가 있는 동안) 재생되는 루프 사운드. 루프 여부는 사운드
	 * 에셋 쪽 설정이고, 여기서는 재생/정지 타이밍만 잡는다 — 첫 탑승자가 들어오면
	 * 재생하고 마지막 탑승자가 나가면 멈춘다(ASecurityCamera::AlarmAudioComponent 와 동일 사유).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<USoundBase> RunningSound;

private:
	/** 지금 이 존 안에 있는 대상들. 매 틱 이만큼만 순회해서 민다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABaseCharacter>> Occupants;

	/**
	 * 지금 이 존 안에 있는 노획물들. 캐릭터와 별도 배열인 이유 —
	 * 노획물은 ALootBase::GetPhysicsRoot() 물리 바디로 움직이지
	 * CharacterMovementComponent 가 없어서, Tick 에서 미는 방식 자체가 다르다
	 * (AddActorWorldOffset 로 순간이동시키면 물리가 깨진다 — SetPhysicsLinearVelocity 를 쓴다).
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ALootBase>> LootOccupants;

	/** 모든 머신에서 RunningSound 재생을 시작한다. 상태를 남기므로(계속 돌아야 함) Reliable 이다 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StartRunningSound();
	void Multicast_StartRunningSound_Implementation();

	/** 모든 머신에서 RunningSound 재생을 멈춘다. 유실되면 소리가 영원히 안 멈추므로 Reliable 이다 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopRunningSound();
	void Multicast_StopRunningSound_Implementation();

	/** 재생 중인 루프 사운드 인스턴스. 재진입 시 중복 재생을 막으려고 들고 있는다 */
	UPROPERTY()
	TObjectPtr<UAudioComponent> RunningAudioComponent;
};
