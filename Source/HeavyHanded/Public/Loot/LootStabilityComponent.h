#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LootStabilityComponent.generated.h"

class ALootBase;
class APawn;

/**
 * 불안정형 노획물. 기울어지면 내용물이 새고 가치가 깎인다. (기획서 5장 — 기울기 60도 초과)
 *
 * [파손형과 다른 점] 파손형은 '사건'(충돌)이지만 불안정형은 '상태'(지금 기울어져 있음)다.
 *   충돌 콜백으로는 잡을 수 없어서 틱으로 본다. 서버에서만, 그리고 계산이 몇 줄뿐이라 싸다.
 *   결과도 다르다 — 파손형은 가치 0 까지 가고 액터가 사라지지만,
 *   불안정형은 MinValueRatio 가 바닥이고 물건은 끝까지 남는다. 손해는 보되 회수는 된다.
 *
 * [기울기를 두 곳에서 얻는다]
 *   놓여 있을 때 : 실제 물리 회전. 넘어지거나 굴러가면 샌다
 *   소지 중      : 운반자의 이동으로 시뮬레이션한다
 *
 *   소지 중에 물리 회전을 읽으면 안 된다. 물리가 꺼진 채 손 소켓에 붙어 있어서
 *   기울기가 '애니메이션이 손을 어디 두느냐'로 정해진다. 플레이어가 통제할 수 없다.
 *   대신 빠르게 오래 움직일수록 쌓이게 해서, 속도를 조절하며 옮기는 압박을 만든다.
 *
 * [경계] 운반자의 속도를 읽기만 하고 플레이어 파트는 건드리지 않는다.
 *   중량형처럼 이동 속도를 깎지 않는다 — 강제가 아니라 압박이다. 얼마나 빨리 갈지는
 *   플레이어가 정하고, 그 대가를 물건이 치른다.
 */
UCLASS(ClassGroup = (Loot), meta = (BlueprintSpawnableComponent))
class HEAVYHANDED_API ULootStabilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULootStabilityComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 지금까지 샌 횟수 */
    UFUNCTION(BlueprintPure, Category = "Loot|Stability")
    int32 GetSpillCount() const { return SpillCount; }

    /** 한 번이라도 샜는가 */
    UFUNCTION(BlueprintPure, Category = "Loot|Stability")
    bool IsSpilled() const { return SpillCount > 0; }

    /** 현재 기울기가 유출 한계에 얼마나 다가갔는가 (0~1). HUD 경고용 */
    UFUNCTION(BlueprintPure, Category = "Loot|Stability")
    float GetTilt01() const { return ReplicatedTilt01 / 255.f; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    /** 이 각도를 넘으면 샌다(도). 수직에서 벗어난 각도이므로 90 이면 완전히 누운 상태다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
        meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float SpillTiltAngle = 60.f;

    /**
     * 이 속도 이하로 움직이면 기울기가 쌓이지 않는다(cm/s).
     * 걷기는 안전하고 뛰면 쌓이는 지점에 둔다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
        meta = (ClampMin = "0.0"))
    float SafeCarrySpeed = 200.f;

    /**
     * 안전 속도 초과분 1cm/s 당 초당 쌓이는 기울기(도).
     *
     * 속도와 시간이 둘 다 들어가는 것이 핵심이다. 얼마나 빠른지(초과분)와
     * 얼마나 오래 그랬는지(누적)가 같이 반영돼야 "조금만 더 뛸까" 하는 판단이 생긴다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
        meta = (ClampMin = "0.0"))
    float TiltGainPerSpeed = 0.06f;

    /** 안전 속도 이하일 때 되돌아오는 속도(도/초). 멈춰서 숨 고르면 회복된다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
        meta = (ClampMin = "0.0"))
    float TiltRecoverRate = 25.f;

    /**
     * 기우는 방향이 이동 방향을 따라가는 속도(도/초).
     *
     * 방향을 즉시 바꾸면 좌우로 왔다 갔다 할 때 물건이 순간이동하듯 꺾인다.
     * 내용물이 쏠렸다가 반대로 쏠리는 데도 시간이 걸린다고 보면 된다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
        meta = (ClampMin = "0.0"))
    float TiltDirectionTurnRate = 180.f;

    /**
     * 놓인 상태에서 이 시간 이상 연속으로 기울어져 있어야 샌다(초).
     *
     * 던지면 ThrowSpinSpeed 로 회전하면서 날아가기 때문에, 순간 각도만 보면
     * 공중에서 새 버린다. 시간으로 한 겹 걸러야 '넘어진 것'과 '회전 중인 것'이 갈린다.
     * 굴러가는 중에는 계속 기울어져 있으므로 그대로 통과한다 — 의도한 동작이다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
        meta = (ClampMin = "0.0"))
    float TiltGraceSeconds = 0.5f;

    /** 기울어져 있는 동안 이 주기로 계속 샌다(초) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
        meta = (ClampMin = "0.1"))
    float SpillIntervalSeconds = 2.f;

    /** 1회 유출당 깎이는 가치 비율 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpillValueLossRatio = 0.15f;

    /**
     * 설계 가치 대비 이 비율 밑으로는 깎이지 않는다.
     * 가치 0 은 파손형의 몫이다. 불안정형까지 0 이 되면 두 특성이 같아진다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinValueRatio = 0.2f;

    /**
     * 소지 중 기울기가 바뀔 때 호출된다. (모든 머신)
     * 흔들림·경고 아이콘·찰랑거리는 사운드는 BP 가 붙인다. 판정은 이미 C++ 에서 끝났다.
     * @param Tilt01  0~1. 1 이면 지금 새는 중이다
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Loot|Stability")
    void OnCarriedTiltChanged(float Tilt01);

    /**
     * 내용물이 샜을 때 호출된다. (모든 머신)
     * 쏟아지는 파티클·사운드용. 실제 가치 차감은 서버가 이미 했다.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Loot|Stability")
    void OnSpilled(int32 NewSpillCount, float TiltDegrees);

private:
    /** 소지 중 기울기를 이동에서 만든다. 반환값은 현재 기울기(도) */
    float UpdateCarriedTilt(const APawn* Carrier, float DeltaTime);

    /** 놓인 상태에서 실제 물리 회전을 읽는다. 반환값은 현재 기울기(도) */
    float GetPhysicalTiltDegrees() const;

    /** 기울기를 받아 유출 여부를 판정한다. 두 모드가 공유한다 */
    void UpdateSpill(float TiltDegrees, float DeltaTime, bool bUseGrace);

    /** 가치를 깎고 연출을 방송한다 (서버 전용) */
    void Spill(float TiltDegrees);

    /** 소지 중 기울어진 모습을 실제로 보여준다. 안 보이면 플레이어가 배울 수 없다 */
    void ApplyCarriedLean(float TiltDegrees);

    /** 이동 방향을 따라가는 기울기 방향(월드, 수평). 관성 방향의 기준이 된다 */
    void UpdateTiltDirection(const APawn* Carrier, float DeltaTime);

    /** 서버가 계산한 기울기를 0~255 로 담아 보낸다 */
    void PushReplicatedTilt(float TiltDegrees);

    UFUNCTION()
    void OnRep_ReplicatedTilt01();

    UFUNCTION()
    void OnRep_SpillCount();

    /** 소유 노획물. UPROPERTY 가 없으면 GC 가 회수한 뒤 엉뚱한 곳에서 크래시한다 */
    UPROPERTY()
    TObjectPtr<ALootBase> OwnerLoot;

    /**
     * 소지 중 누적된 기울기(도). 서버에서만 쓴다.
     * 놓는 순간 실제 물리 회전으로 넘어가므로 그때 0 으로 돌린다.
     */
    float CarriedTiltDegrees = 0.f;

    /** 지금 기울기가 한계를 넘긴 채 지난 시간(초). 그레이스 판정용 */
    float TiltedSeconds = 0.f;

    /**
     * 관성이 작용하는 방향(월드, 수평 단위벡터). 대개 이동 방향과 같다.
     * 물건 윗부분은 이 반대쪽으로 넘어간다.
     * 멈춰 있는 동안에는 마지막 방향을 유지해야 기울기가 제자리에서 돌지 않는다.
     */
    FVector TiltDirection = FVector::ZeroVector;

    /** 마지막으로 샌 시각(서버 기준). 초기값은 첫 유출이 즉시 나가도록 크게 잡는다 */
    float LastSpillTime = -BIG_NUMBER;

    /**
     * 복제되는 기울기 비율(0~255).
     *
     * float 를 그대로 복제하면 걷는 내내 매 프레임 dirty 가 된다.
     * 표시용이라 0.4%p 해상도면 충분하다. (소음 파트가 경계도에 쓰는 방식과 같다)
     */
    UPROPERTY(ReplicatedUsing = OnRep_ReplicatedTilt01, VisibleInstanceOnly, Category = "Loot|Stability")
    uint8 ReplicatedTilt01 = 0;

    UPROPERTY(ReplicatedUsing = OnRep_SpillCount, VisibleInstanceOnly, Category = "Loot|Stability")
    int32 SpillCount = 0;
};
