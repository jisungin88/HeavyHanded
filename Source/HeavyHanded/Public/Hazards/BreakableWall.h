#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakableWall.generated.h"

class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;
class UMaterialInstanceDynamic;

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
 * [HP 제(HitPoints) — ULootDurabilityComponent 와 같은 패턴]
 *   파손형 노획물이 "충격 누적 → 균열 → 파괴" 를 이미 이 방식으로 구현해 뒀다.
 *   벽도 같은 문제(몇 번 맞아야 부서지는가 + 그 사이 시각 피드백)라 새로 설계하지 않고
 *   그대로 가져왔다 — CrackParameterName(기본 "CrackAmount") 도 같은 이름을 쓴다.
 *   나중에 벽과 노획물이 같은 머티리얼 계열을 공유하게 되면 파라미터 이름이 이미 맞아 있다.
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
	 * 이 충격이 벽에 타격 하나를 입힌다. (서버 전용)
	 *
	 * HitPoints 번 맞아야 부서진다 — 한 번 부딪혔다고 바로 안 부서질 수 있다.
	 * 부서지기 전까지는 맞을 때마다 ApplyCrackVisual() 로 균열이 진행된다.
	 *
	 * @param Breacher       때린 주체(브루트 캐릭터 또는 폭탄). 원인 구분·로그용, 없어도 된다
	 * @param ImpactLocation 충격 위치
	 * @param ImpactRadius   폭발 반경. 0 이면 직접 타격(돌진)으로 보고 거리 판정을 생략한다
	 * @return 이번 타격으로 완전히 부서졌으면 true. 범위 밖이거나 아직 HP 가 남았으면 false
	 */
	bool TryBreak(const AActor* Breacher, const FVector& ImpactLocation, float ImpactRadius = 0.f);

	UFUNCTION(BlueprintPure, Category = "Hazard|Break")
	bool IsBroken() const { return bIsBroken; }

	/** 지금까지 누적된 타격 횟수 */
	UFUNCTION(BlueprintPure, Category = "Hazard|Break")
	int32 GetHitCount() const { return HitCount; }

	/**
	 * 파손 진행도 (0~1). ULootDurabilityComponent::GetDamageRatio01() 과 같은 식이다 —
	 * "1 은 깨진 상태가 아니라 다음 타격에 깨진다" 는 뜻. 분모가 HitPoints 보다 하나
	 * 적은 이유도 동일하다 — 마지막 타격은 같은 프레임에 메시가 숨겨져 1.0 이 화면에
	 * 안 나오기 때문에, 하나 적게 나눠서 "파괴 직전"이 정확히 1.0 이 되게 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Hazard|Break")
	float GetDamageRatio01() const;

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

	/**
	 * 부서지기까지 필요한 타격 횟수. 기본 2 — 브루트 돌진이든 점착 폭탄이든 두 번 맞아야 뚫린다.
	 * 1 로 두면 예전처럼 한 방에 부서진다(균열 단계 없음).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Break",
		meta = (ClampMin = "1"))
	int32 HitPoints = 2;

	/**
	 * 균열 머티리얼의 스칼라 파라미터 이름. ULootDurabilityComponent::CrackParameterName 과
	 * 같은 관례다 — 타격이 쌓일 때마다 0~1 로 설정된다. None 이면 균열 연출을 하지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Break|Visual")
	FName CrackParameterName = TEXT("CrackAmount");

	/**
	 * 타격이 하나 쌓였을 때(아직 안 부서졌을 때) 호출된다. 균열 단계 연출용.
	 * 머티리얼 균열은 C++ 이 이미 적용했으므로 BP 는 사운드·파티클만 붙인다
	 * (ULootDurabilityComponent::OnDamageAccumulated 와 동일한 역할 분담).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hazard|Break")
	void OnDamageAccumulated(int32 NewHitCount, int32 MaxHitPoints);

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

	UFUNCTION()
	void OnRep_HitCount();

	/** 연출을 내고 벽 지우기를 예약한다. 모든 머신에서 실행된다 */
	void ApplyBreak();

	/** 벽과 그 아래 붙은 것을 시야·콜리전·내비게이션에서 뺀다. 모든 머신에서 실행된다 */
	void HideWall();

	/**
	 * 균열 정도를 머티리얼에 반영한다. 모든 머신에서 실행된다.
	 * ULootDurabilityComponent::ApplyCrackVisual() 과 동일 로직 — 동적 인스턴스는
	 * 첫 타격 때 한 번만 만든다. 안 맞은 벽은 MID 를 하나도 들지 않는다.
	 */
	void ApplyCrackVisual();

	/**
	 * 부서졌는가. 서버가 정하고 복제된다.
	 * 서버에서 직접 대입하면 RepNotify 가 안 불리므로 OnRep 을 손으로 부른다 (AVaultDoor 와 동일 사유).
	 */
	UPROPERTY(ReplicatedUsing = OnRep_bIsBroken, VisibleInstanceOnly, Category = "Hazard")
	bool bIsBroken = false;

	/** 누적 타격 횟수. 클라이언트는 이 값으로 균열 단계를 맞춘다 */
	UPROPERTY(ReplicatedUsing = OnRep_HitCount, VisibleInstanceOnly, Category = "Hazard")
	int32 HitCount = 0;

	/** 슬롯별 동적 머티리얼 인스턴스. 첫 타격 때 만들어 재사용한다 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CrackMaterials;

	/** 균열 파라미터를 가진 슬롯이 하나도 없다는 경고를 이미 냈는가 */
	bool bWarnedMissingCrackParameter = false;

	FTimerHandle HideTimer;
};
