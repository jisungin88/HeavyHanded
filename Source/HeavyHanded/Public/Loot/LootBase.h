#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/HeavyHandedTypes.h"
#include "Interfaces/Carryable.h"
#include "LootBase.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class APawn;
struct FPredictProjectilePathResult;

/**
 * 모든 노획물의 베이스.
 *
 * [설계 원칙 1] 값은 데이터, 행동은 컴포넌트.
 *   중량형·파손형·불안정형을 서브클래스로 나누지 않는다.
 *   중량형은 고유 행동이 없고 FLootPhysicsData 값만 다르며,
 *   파손형·불안정형은 각자 컴포넌트를 붙여 만든다.
 *   대형 금고처럼 '중량형 + 경보 연동형' 조합이 반드시 생기므로 상속하면 클래스가 폭발한다.
 *   (미션 가이드도 "인터페이스와 Actor Component를 활용한 설계"를 요구한다)
 *
 * [설계 원칙 2] 충돌 게이팅은 여기 한 곳에서만 한다.
 *   물리 낙하 1회에 OnHit 은 5~15회 발생한다. 튕김·구름·미세 접촉이 전부 개별 콜백으로 온다.
 *   ALootBase 가 [임계값 + 디바운스]로 걸러 '확정 충격 1개'를 만들고,
 *   그 하나를 OnLootImpact 로 방송해 소음 파트와 파손 컴포넌트가 함께 소비한다.
 *   파손 컴포넌트가 raw OnHit 을 따로 세면 파손형이 한 번 낙하로 즉사한다.
 *
 * 서버 권위 + 클라이언트 보간. 클라이언트 예측은 쓰지 않는다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API ALootBase : public AActor, public ICarryable
{
    GENERATED_BODY()

public:
    ALootBase();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    //~ ICarryable 시작 — 플레이어는 요청하고, 아이템이 허용/거부한다
    virtual EWeightClass GetWeightClass() const override;
    virtual int32 GetRequiredCarriers() const override;
    virtual float GetCarrySpeedMultiplier() const override;
    virtual bool IsJumpAllowedWhileCarried() const override;
    virtual bool CanBeCarriedBy(const APawn* Requester) const override;
    virtual void OnGrabbed(APawn* Carrier) override;
    virtual void OnReleased(APawn* Carrier) override;
    virtual bool CanBeThrown() const override;
    virtual void OnThrown(APawn* Carrier, const FVector& AimDirection) override;
    virtual APawn* GetPrimaryCarrier() const override;
    virtual UPrimitiveComponent* GetPhysicsRoot() const override;
    //~ ICarryable 끝

    /**
     * 던졌을 때의 발사 속도. 조준 방향 + 포물선 성분 + 운반자 속도까지 합친 최종 값이다.
     *
     * 서버의 임펄스와 클라이언트의 궤적 표시가 이 함수 하나를 공유한다.
     * 계산을 따로 두면 미리 보이는 궤적과 실제로 날아가는 경로가 어긋난다.
     */
    FVector ComputeThrowVelocity(const FVector& AimDirection) const;

    /**
     * 던지기 궤적을 예측한다. 조준 중인 클라이언트가 로컬로 그리는 표시용이다.
     *
     * 클라이언트 예측이 아니다 — 결과를 서버에 보내지 않고, 실제 판정은 서버가 다시 한다.
     * 표시가 실제와 다르면 그건 표시가 틀린 것이지 게임 상태가 갈린 것이 아니다.
     */
    bool PredictThrowPath(const FVector& AimDirection, FPredictProjectilePathResult& OutResult);

    /**
     * 게이팅을 통과한 '확정 충격'만 방송된다. 서버에서만 발생한다.
     *
     * 구독자는 두 종류다. 둘 다 같은 이벤트를 소비한다.
     *   - 소음 파트  : 이 충격이 얼마나 시끄러운지 해석한다 (여기서는 판단하지 않는다)
     *   - 파손 컴포넌트: DamageImpulseThreshold 이상만 골라 누적한다
     */
    FOnLootImpactSignature OnLootImpact;

    /** 물리·운반 수치. 파손 컴포넌트 등이 임계값을 읽어간다 */
    const FLootPhysicsData& GetPhysicsData() const { return PhysicsData; }

    /**
     * 다음 확정 충격의 원인을 예약한다. (서버 전용)
     * 놓기·던지기 직후 첫 충돌을 Drop / Throw 로 표시하기 위한 것으로,
     * 한 번 소비되면 다시 Collision 으로 돌아간다.
     * 던지기 단계에서 플레이어를 InInstigator 로 넘겨 '최다 소음 유발자' 집계에 쓴다.
     */
    void SetPendingImpactCause(ELootImpactCause InCause, APawn* InInstigator);

protected:
    virtual void BeginPlay() override;

    /** 물리 바디이자 루트. 플레이어 파트가 Attach 대상으로 쓴다 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
    TObjectPtr<UStaticMeshComponent> LootMesh;

    /**
     * 무게·질량·임계값. 지금은 액터에 인라인으로 두고, 마지막 단계에서 DataAsset 으로 뺀다.
     * BP 자식 클래스가 값만 지정하는 껍데기가 되도록 EditAnywhere 로 연다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
    FLootPhysicsData PhysicsData;

    /**
     * 소지 중 어태치할 운반자 스켈레탈 메시의 소켓 이름.
     *
     * 캐릭터 리깅이 바뀌거나 파트별로 손 소켓 이름이 달라질 수 있으므로 코드에 박지 않는다.
     * BP 자식 클래스에서 노획물마다 다른 소켓(예: 양손 물건은 별도 소켓)을 지정할 수도 있다.
     * 소켓을 찾지 못하면 스켈레탈 메시 원점에 붙는다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Carry")
    FName CarrySocketName = TEXT("hand_r");

    /**
     * 같은 대상에 대한 재충돌을 이 시간 동안 무시한다.
     * 임계값만으로는 부족하다 — 세게 떨어지면 강한 충격이 연달아 여러 번 잡힌다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Impact",
        meta = (ClampMin = "0.0"))
    float ImpactDebounceSeconds = 0.3f;

    /**
     * 현재 이 노획물을 들고 있는 대표(리더).
     * 2인 협력 캐리에서도 소유·이동을 결정하는 쪽은 항상 리더 한 명이다.
     * 물건 하나에 두 플레이어가 물리 제약을 거는 방식은 네트워크에서 깨진다.
     */
    UPROPERTY(ReplicatedUsing = OnRep_PrimaryCarrier, VisibleInstanceOnly, Category = "Loot|Carry")
    TObjectPtr<APawn> PrimaryCarrier;

    UFUNCTION()
    void OnRep_PrimaryCarrier();

    /** 물리 ON/OFF, 콜리전 프로파일, 어태치, 운반자 상호 무시를 소지 상태에 맞춘다. 모든 머신에서 실행된다 */
    virtual void ApplyCarryState();

    /** 운반자의 손 소켓에 어태치한다. 물리를 끈 뒤에 부른다 */
    void AttachToCarrier(APawn* Carrier);

    UFUNCTION()
    void HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
    /** 디바운스 통과 여부를 판정하고, 통과하면 발생 시각을 기록한다 */
    bool TryConsumeImpactCooldown(const AActor* OtherActor, float Now);

    /** 운반자 캡슐과 노획물이 서로의 이동 스윕을 무시하도록 설정/해제한다 */
    void SetCarrierMoveIgnore(APawn* Carrier, bool bIgnore);

    /** 대상별 마지막 확정 충격 시각. 키가 죽으면 정리된다 */
    TMap<TWeakObjectPtr<const AActor>, float> RecentImpactTimes;

    /** 상호 무시를 걸어 둔 운반자. 운반자가 바뀔 때 해제 대상을 놓치지 않기 위해 따로 들고 있는다 */
    TWeakObjectPtr<APawn> MoveIgnoredCarrier;

    /** SetPendingImpactCause 로 예약된 원인. 확정 충격 1회에 소비된다 */
    ELootImpactCause PendingImpactCause = ELootImpactCause::Collision;

    /** 예약된 원인 제공자 */
    TWeakObjectPtr<APawn> PendingInstigatorPawn;
};
