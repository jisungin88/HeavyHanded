#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MovementTrap.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraSystem;
class USoundBase;
class ABaseCharacter;

/**
 * 밟으면 한동안 움직이지 못하게 하는 덫. (기획서 3장 "트랩 작동" — 대, +15%, 30m)
 * bDestroyAfterTrigger 로 곰덫(재사용)과 바나나 껍질(1회 소모) 둘 다 이 클래스 하나로 만든다 —
 * 메커니즘이 완전히 같아서(밟으면 잠깐 못 움직인다) BP 값만 다르게 두는 쪽을 택했다.
 *
 * [GAS 가 아니라 UCharacterMovementComponent 를 직접 잠근다]
 *   이 프로젝트의 기존 이동 잠금(UGA_HeavyCarryAssist::StaggeredMovementLockEffectClass)은
 *   GameplayEffect 를 쓴다. 이 클래스는 일부러 그 경로를 안 탄다 — GE 를 쓰려면 새 에셋과
 *   "덫에 걸렸다" 를 알릴 새 State 태그가 필요한데, 그 태그는 전영배 소유(State.ini)라
 *   Hazard 시스템 혼자 판단할 일이 아니다.
 *
 *   대신 CharacterMovementComponent::DisableMovement() 는 엔진이 공개한 API 이고, 이동
 *   모드는 서버→클라 복제가 이미 CMC 안에서 처리된다. 서버 권위 규칙을 지키면서도
 *   플레이어 캐릭터 파일을 한 줄도 안 건드리는 방법이 이것뿐이었다.
 *
 *   ⚠️ 부작용 — MOVE_None 은 중력도 멈춘다. 바닥에 서 있는 상태에서 걸리는 것을 전제로
 *   한다. 공중에서 덫에 걸리는 경우(예: 낙하 중 판정)는 고려하지 않았다.
 *
 * [경비는 안 걸린다]
 *   OtherActor 를 ABaseCharacter 로만 캐스트한다. AGuardCharacter 는 별도 클래스라
 *   자기 구역의 덫에 순찰 중 걸리는 일이 생기지 않는다.
 *
 * [소음은 여기서 낸다 — ABreakableWall 과 다른 점]
 *   벽은 "누가 부쉈는지" 가 호출자 책임이라 소음을 안 냈지만, 덫은 이 자체가 사건의
 *   시작이라 다른 발행 주체가 없다. Noise.Hazard.Trap 은 이미 Noise.ini(지성인)에
 *   등록돼 있는 태그를 그대로 참조한 것이다 — 새로 만들지 않았다.
 *
 * [연출은 Multicast — 상태가 아니라 순간이다]
 *   bArmed 는 서버 전용 판정이고 복제하지 않는다. 클라이언트가 봐야 하는 건 "지금 걸렸다"
 *   는 순간의 연출뿐이라 CLAUDE.md 3절 규칙대로 Unreliable Multicast 로 처리한다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API AMovementTrap : public AActor
{
	GENERATED_BODY()

public:
	AMovementTrap();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/**
	 * 눈에 보이는 덫 메시. 루트다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> TrapMesh;

	/**
	 * 밟았는지 판정하는 트리거 볼륨. 메시보다 살짝 작게 잡아야 스치기만 해도
	 * 걸리는 것을 피할 수 있다 — 정확한 값은 BP 에서 메시에 맞춰 조정할 것.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/** 걸린 대상이 움직이지 못하는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Trap",
		meta = (ClampMin = "0.0", Units = "s"))
	float ImmobilizeDuration = 3.f;

	/**
	 * 풀려난 뒤 다시 작동하기까지의 대기 시간. 0 이면 풀려나자마자 재무장한다.
	 * 같은 대상이 그 자리에 계속 서 있어도 재중첩(BeginOverlap) 없이는 재발동하지 않는다.
	 * bDestroyAfterTrigger 가 true 면 이 값은 안 쓰인다 — 재무장하지 않고 사라지기 때문이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Trap",
		meta = (ClampMin = "0.0", Units = "s", EditCondition = "!bDestroyAfterTrigger"))
	float ResetDelay = 2.f;

	/**
	 * true 면 한 번 걸린 뒤 대상을 풀어주고 재무장하지 않은 채 스스로 사라진다.
	 * 바나나 껍질처럼 한 번 쓰면 없어지는 소모성 함정에 켠다. false(기본)면 곰덫처럼
	 * ResetDelay 뒤 다시 작동한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Trap")
	bool bDestroyAfterTrigger = false;

	// ---- 연출 (BP 는 에셋만 고른다) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<UNiagaraSystem> TriggerEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<USoundBase> TriggerSound;

private:
	/** 걸리는 순간의 연출만 전달한다. 상태를 남기지 않으므로 Unreliable 이다 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayTriggerEffect();
	void Multicast_PlayTriggerEffect_Implementation();

	/** ImmobilizeDuration 뒤 호출돼 이동을 되돌린다 */
	void ReleaseTarget(TWeakObjectPtr<ABaseCharacter> TargetPtr);

	/** ImmobilizeDuration + ResetDelay 뒤 호출돼 다시 작동 가능하게 한다 */
	void Rearm();

	FTimerHandle ReleaseTimer;
	FTimerHandle RearmTimer;

	/** 서버 전용 판정. 복제하지 않는다 — 클라는 결과(연출)만 보면 된다 */
	bool bArmed = true;
};
