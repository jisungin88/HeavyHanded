#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakableWall.generated.h"

class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 정해진 구간만 부서지는 벽. 브루트의 벽 뚫기 돌진과 점착 폭탄 둘 다의 대상이 된다.
 * (기획서 4-1장 — Ability.Brute.WallCharge / 7장 — 점착 폭탄)
 *
 * [AVaultDoor 와 같은 원칙 — 판정은 벽이 한다]
 *   돌진이든 폭발이든 "여기서 이렇게 부딪혔다/터졌다" 만 알리고(TryBreak),
 *   그것으로 부서지는지는 이 벽이 정한다. 호출하는 쪽은 벽의 크기·모양을 몰라도 된다.
 *
 * [메시 하나가 전부다 — AVaultDoor 의 프레임에 해당하는 것이 없다]
 *   금고 문은 뚜껑만 사라지고 프레임(벽의 일부)이 남아야 하지만, 이 벽은 레벨
 *   디자이너가 "여기가 부서지는 자리" 라고 통째로 배치하는 조각이다. 그래서
 *   구멍 뚫린 콜리전을 걱정할 필요가 없다 — 부서지면 이 메시 전체가 사라지고
 *   그 자리가 곧 뚫린 통로다. 주변 벽(안 부서지는 부분)은 레벨 지오메트리로 남는다.
 *
 * [소음은 여기서 내지 않는다]
 *   ALootBase 와 같은 원칙 — 이 클래스는 파괴 판정과 연출만 한다.
 *   Noise.Structure.Break 발행은 이 벽을 부순 쪽(브루트 어빌리티/폭탄)의 책임이다.
 *   그래야 "누가 얼마나 시끄러웠는가" 집계가 항상 행위자 기준으로 남는다.
 *
 * [ImpactRadius 0 은 직접 타격이다]
 *   브루트 돌진처럼 몸으로 부딪히는 경우 거리 판정이 필요 없다 — 닿았으면 그걸로 끝이다.
 *   점착 폭탄처럼 반경으로 터지는 경우만 BreakTolerance 와 비교한다.
 *
 * 서버 권위 + 복제. 판정은 서버가 하고 연출은 각 머신이 각자 돌린다. (AVaultDoor 와 동일)
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API ABreakableWall : public AActor
{
	GENERATED_BODY()

public:
	ABreakableWall();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 이 충격이 벽을 부수는가. (서버 전용)
	 *
	 * @param Breacher       부순 주체(브루트 캐릭터 또는 폭탄). 원인 구분·로그용, 없어도 된다
	 * @param ImpactLocation 충격 위치
	 * @param ImpactRadius   폭발 반경. 0 이면 직접 타격(돌진)으로 보고 거리 판정을 생략한다
	 * @return 이번 호출로 부서졌으면 true. 이미 부서졌거나 범위 밖이면 false
	 */
	bool TryBreak(const AActor* Breacher, const FVector& ImpactLocation, float ImpactRadius = 0.f);

	UFUNCTION(BlueprintPure, Category = "Hazard|Break")
	bool IsBroken() const { return bIsBroken; }

protected:
	virtual void BeginPlay() override;

	/**
	 * 부서지는 벽 조각. 루트이고 부서지면 이것과 그 아래 붙은 것 전부 사라진다.
	 *
	 * 레벨 디자이너가 이 액터를 "부서지는 자리" 로 배치한다. 나머지 벽은 이 액터와
	 * 무관한 레벨 지오메트리로 남는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> WallMesh;

	/**
	 * 폭발이 이 벽을 부수는 것으로 인정하는 거리. 표면 기준이다 (AVaultDoor::BreachRadius 와 동일 원칙).
	 * ImpactRadius 가 0(직접 타격)이면 쓰이지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Break",
		meta = (ClampMin = "0.0", Units = "cm"))
	float BreakTolerance = 100.f;

	// ---- 연출 (BP 는 에셋만 고른다. AVaultDoor 의 부서짐 연출과 같은 구조) ----

	/** 벽이 부서지는 연출. BP_VaultDoor 의 BreachEffect 와 같은 역할 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<UNiagaraSystem> BreakEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual",
		meta = (ClampMin = "0.1"))
	float BreakEffectScale = 1.f;

	/**
	 * 이펙트를 벽 중심에서 액터 정면으로 얼마나 밀어낼지.
	 *
	 * 기준점은 WallMesh 의 **바운즈 중심**이다(피벗이 아니다 — AVaultDoor 와 같은 이유).
	 * 액터는 부서진 자리가 뚫려야 할 쪽(방 안쪽)으로 정면(+X)을 맞춰 배치할 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual",
		meta = (Units = "cm"))
	float BreakEffectForwardOffset = 30.f;

	/** 벽이 부서지는 소리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<USoundBase> BreakSound;

	/**
	 * 연출이 나온 뒤 이만큼 지나서 벽을 지운다. AVaultDoor::DoorHideDelay 와 같은 이유 —
	 * 먼지가 화면을 가리는 동안 지워서 "팍 사라지는" 순간 자체를 아무도 보지 못하게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual",
		meta = (ClampMin = "0.0", Units = "s"))
	float WallHideDelay = 0.15f;

private:
	UFUNCTION()
	void OnRep_bIsBroken();

	/** 연출을 내고 벽 지우기를 예약한다. 모든 머신에서 실행된다 */
	void ApplyBreak();

	/** 벽과 그 아래 붙은 것을 시야·콜리전·내비게이션에서 뺀다. 모든 머신에서 실행된다 */
	void HideWall();

	/**
	 * 부서졌는가. 서버가 정하고 복제된다.
	 * 서버에서 직접 대입하면 RepNotify 가 안 불리므로 OnRep 을 손으로 부른다 (AVaultDoor 와 동일 사유).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_bIsBroken, VisibleInstanceOnly, Category = "Hazard")
	bool bIsBroken = false;

	FTimerHandle HideTimer;
};
