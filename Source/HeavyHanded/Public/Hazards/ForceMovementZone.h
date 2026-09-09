#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForceMovementZone.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class ABaseCharacter;

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
 *   ⚠️ 알려진 한계 — 플레이어 자신의 WASD 이동과는 별도 경로로 밀기 때문에, 반대 방향으로
 *   걸어도 완전히 이기지 못할 수 있다(단순히 이동을 더하는 구조라 서로 상쇄되지 않는다).
 *   Base 버전에서는 이 정도로 두고, 어색하면 그때 다시 설계한다.
 *
 * [경비는 안 걸린다]
 *   OtherActor 를 ABaseCharacter 로만 캐스트해 Occupants 에 담는다. 다른 Hazard
 *   클래스들과 같은 이유로, 경비가 순찰 중 컨베이어에 실려 엉뚱한 곳으로 안 간다.
 *
 * 서버 권위 — 미는 판정은 서버에서만 한다. 결과(캐릭터 위치)는 이동 복제로 전파된다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API AForceMovementZone : public AActor
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

private:
	/** 지금 이 존 안에 있는 대상들. 매 틱 이만큼만 순회해서 민다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABaseCharacter>> Occupants;
};
