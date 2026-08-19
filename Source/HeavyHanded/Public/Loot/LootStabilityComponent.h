#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Loot/LootTypes.h"              // FLootStabilityData 를 값으로 보유 — 전방 선언 불가
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

    /**
     * 설계 수치. DT_LootStability 에서 노획물의 행 이름으로 찾아 채운다.
     *
     * [왜 컴포넌트가 직접 조회하나]
     *   이 값을 읽는 것은 여기뿐이다. 예전처럼 ALootBase 가 들고 있으면 불안정형이 아닌
     *   노획물까지 전부 9개 필드를 갖게 되고, 카탈로그 표에도 그 열이 생겨서
     *   "이 물건도 기울면 새나?" 로 읽힌다.
     *
     * [BP 에서 고칠 수 있게 열어 두는 이유]
     *   표에 행이 없으면 여기 적힌 값이 그대로 쓰인다. ALootBase 가 PhysicsData 에 대해
     *   하는 것과 같은 폴백이고, 표에 안 올린 실험물을 만들 길이다.
     *   행이 있으면 ResolveData 가 덮어쓰므로 여기 값은 무시된다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability")
    FLootStabilityData Data;

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
    /**
     * DT_LootStability 에서 자기 행을 찾아 Data 를 채운다. 한 번만 실제로 돈다.
     *
     * 클라이언트에서도 부른다. 표 조회는 모든 머신에서 같은 답이 나오는 순수 계산이라
     * 수치를 복제할 이유가 없다 — 복제하는 것은 결과(기울기·유출 횟수)뿐이다.
     */
    void ResolveData();

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

    /** ResolveData 가 이미 돌았는가. 표 조회를 매번 반복하지 않기 위한 것이다 */
    bool bDataResolved = false;

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
