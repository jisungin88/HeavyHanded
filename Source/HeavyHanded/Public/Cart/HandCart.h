#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HandCart.generated.h"

class ALootBase;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

/**
 * 핸드카트(대차). 노획물을 담아 옮기는 장비. (기획서 7장 — $20,000)
 *
 * [카트를 사는 이유는 '인원을 푸는 것' 이다]
 *   중량형은 여전히 2인이어야 들린다. 카트가 그 규칙을 깨지 않는다.
 *   두 사람이 함께 들어서 카트에 싣고, 그 뒤로는 한 명이 끌고 간다.
 *   카트가 없으면 두 명이 밴까지 계속 붙잡혀 있어야 하는데, 있으면 한 명이 풀려난다.
 *   기획서의 "중량형을 1인이 밀어서 운반" 은 운반이 1인이라는 뜻이지
 *   적재까지 1인이라는 뜻이 아니다. (2026-08-20 결정)
 *
 * [담긴 물건은 물리를 유지한다 — 어태치하지 않는다]
 *   들고 있을 때(ALootBase::ApplyCarryState)는 물리를 끄고 붙이지만 카트는 반대다.
 *   물건이 카트 안에서 계속 흔들리고, 험하게 몰면 밖으로 쏟아진다.
 *   그 '쏟아짐' 이 카트의 유일한 위험 요소라서, 물리를 끄면 게임이 사라진다.
 *
 *   대신 덜그럭거리는 소리와 파손을 막아야 한다. 안 막으면 소음을 줄이려고 산 장비가
 *   소음 발생기가 되고, 파손형은 타고 가는 것만으로 깨진다.
 *   → ALootBase::SetContainingCart 가 그 두 가지를 끈다.
 *
 * [노획물 3종이 카트와 각각 다른 관계를 갖는다]
 *   파손형     안 깨진다      (충격 보고를 끄므로 누적되지 않는다)
 *   불안정형   샌다          (유출은 기울기로 판정하지 벽 충돌로 판정하지 않는다.
 *                            그래서 아무것도 안 해도 살아 있다 — 의도한 것이다)
 *   중량형     실을 수 있다   (단, 싣는 데 2인)
 *
 * [벽에 막히면 미는 사람도 막힌다]
 *   카트는 물리 바디이고 Pawn 채널을 Block 한다. 운반자에게 이동 무시를 걸어 주는
 *   노획물과 정반대다 — 노획물은 자기가 든 물건에 막히면 안 되지만,
 *   카트는 막혀야 "좁은 통로 불가" 라는 기획서상 유일한 단점이 성립한다.
 *   그래서 IgnoreActorWhenMoving 을 걸지 않는다. 콜리전이 알아서 한다.
 *
 * 서버 권위 + 클라이언트 보간. 노획물과 같은 정책이다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API AHandCart : public AActor
{
	GENERATED_BODY()

public:
	AHandCart();

	/** 지금 이 카트에 실려 있는 노획물. 서버에서만 채워진다 */
	const TArray<TObjectPtr<ALootBase>>& GetContainedLoot() const { return ContainedLoot; }

	UFUNCTION(BlueprintPure, Category = "Cart")
	int32 GetContainedCount() const { return ContainedLoot.Num(); }

	UFUNCTION(BlueprintPure, Category = "Cart")
	bool IsContaining(const ALootBase* Loot) const;

	/**
	 * 이 노획물을 적재 목록에서 뺀다. (서버 전용)
	 *
	 * 볼륨을 벗어나면 저절로 빠지지만, 적재면 위에서 그대로 집어 올리는 경우가 있다.
	 * 그때는 볼륨 안에 머문 채 사람 손에 들리므로 EndOverlap 이 오지 않는다.
	 * 그래서 ALootBase::OnGrabbed 가 이 함수를 직접 부른다.
	 */
	void ReleaseLoot(ALootBase* Loot);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 물리 바디이자 루트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cart")
	TObjectPtr<UStaticMeshComponent> CartMesh;

	/**
	 * 적재면 위 공간. 여기 들어온 노획물을 '실린 것' 으로 센다.
	 *
	 * 메시가 임시라 기본값은 대략치다. 실제 카트 메시가 들어오면 BP 에서 적재면에 맞춘다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cart")
	TObjectPtr<UBoxComponent> LoadVolume;

	/** 이 세기 미만의 충돌은 소음으로 치지 않는다. 밀고 다닐 때의 미세 접촉을 거른다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cart|Noise", meta = (ClampMin = "0.0"))
	float NoiseImpulseThreshold = 600.f;

	/** 이 세기면 프로파일 기본 크기를 그대로 낸다. 그 아래는 비례해 줄어든다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cart|Noise", meta = (ClampMin = "1.0"))
	float NoiseFullImpulse = 4000.f;

	/** 같은 대상에 대해 이 시간 안에는 다시 발행하지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cart|Noise", meta = (ClampMin = "0.0", Units = "s"))
	float NoiseDebounceSeconds = 0.3f;

	/** 카트 자체의 질량(kg). 실린 물건 무게는 물리 엔진이 따로 더한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cart|Physics", meta = (ClampMin = "1.0"))
	float MassKg = 60.f;

	/**
	 * 앞뒤·좌우로 넘어지는 것을 막는다.
	 *
	 * 물리 바디라 급회전하면 뒤집힌다. 재밌을 수도 있지만 처음부터 열어 두면
	 * "왜 자꾸 뒤집히지" 로 시간을 쓴다. 잠가 두고 나중에 풀어 보는 편이 낫다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cart|Physics")
	bool bLockTipping = true;

private:
	UFUNCTION()
	void HandleLoadBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleLoadEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/**
	 * 카트 몸체가 무언가에 부딪혔다. 벽에 박은 것만 소음으로 낸다.
	 *
	 * [UNoiseEmitterComponent 를 안 쓰고 직접 거는 이유]
	 *   그 컴포넌트는 자기가 알아서 모든 OnComponentHit 을 잡는다. 편해서 노획물에는 그대로
	 *   붙였지만 카트에는 못 쓴다 — 실려 있는 물건이 카트 바닥에 부딪히는 것까지 소음이 되고,
	 *   그러면 물건 쪽 소음을 아무리 막아도 카트가 대신 시끄럽다.
	 *   무엇에 부딪혔는지를 봐야 하는데 그 판단을 끼워 넣을 자리가 저쪽에는 없다.
	 *
	 *   그래서 여기서 걸러 UNoiseSubsystem::ReportNoise 로 직접 보낸다.
	 *   임계값과 재발행 차단은 ALootBase 가 쓰는 것과 같은 두 겹 구조다.
	 */
	UFUNCTION()
	void HandleCartHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** 이 충돌을 소음으로 칠 것인가. 실려 있는 물건과 사람은 제외한다 */
	bool ShouldReportHitAsNoise(const AActor* OtherActor) const;

	/** 대상별 마지막 발행 시각. 짧은 시간 내 재발행을 막는다 */
	TMap<TWeakObjectPtr<const AActor>, float> RecentNoiseTimes;

	/** 적재 목록에 넣고 노획물 쪽 상태를 켠다. (서버 전용) */
	void ContainLoot(ALootBase* Loot);

	/**
	 * 실려 있는 노획물. 서버에서만 유효하다.
	 *
	 * 복제하지 않는 이유: 이 목록으로 하는 일(소음 억제 · 파손 제외)이 전부 서버 판정이다.
	 * 클라이언트가 "이 물건이 카트에 실렸는가" 를 알아야 할 때는 노획물 쪽
	 * ALootBase::ContainingCart 가 복제되므로 그것을 본다.
	 *
	 * UPROPERTY 가 없으면 GC 가 회수한 뒤 엉뚱한 곳에서 크래시한다.
	 */
	UPROPERTY()
	TArray<TObjectPtr<ALootBase>> ContainedLoot;
};
