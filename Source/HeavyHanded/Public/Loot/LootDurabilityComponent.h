#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LootDurabilityComponent.generated.h"

class ALootBase;
struct FLootImpactEvent;

/**
 * 파손형 노획물의 내구도. 충격이 누적되면 깨지면서 사라진다. (기획서: 충격 3회)
 *
 * [경계] 이 컴포넌트는 raw OnHit 을 직접 듣지 않는다.
 *   ALootBase 가 게이팅해 만든 '확정 충격'만 구독한다.
 *   물리 낙하 1회에 OnHit 은 5~15회 발생하므로, 직접 세면 한 번 떨어뜨렸을 때
 *   즉사한다. 기획서의 '3회'는 의미 있는 충격 3번이지 콜백 3개가 아니다.
 *
 * [임계값이 2개인 이유]
 *   ImpactReportThreshold(200)  = 소음 파트에 알릴 최소 충격
 *   DamageImpulseThreshold(600) = 파손으로 칠 최소 충격
 *   살짝 부딪히는 소리는 나야 하지만 그게 파손까지 되면 안 된다.
 *
 * 값은 ALootBase 의 FLootPhysicsData 에서 읽는다. 여기에 수치를 중복해서 두지 않는다.
 */
UCLASS(ClassGroup = (Loot), meta = (BlueprintSpawnableComponent))
class HEAVYHANDED_API ULootDurabilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULootDurabilityComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 파괴되었는가. 파괴 직후 액터가 사라지므로 참조하는 쪽은 IsValid 를 먼저 봐야 한다 */
    UFUNCTION(BlueprintPure, Category = "Loot|Durability")
    bool IsBroken() const { return bIsBroken; }

    /** 지금까지 누적된 충격 횟수 */
    UFUNCTION(BlueprintPure, Category = "Loot|Durability")
    int32 GetImpactCount() const { return ImpactCount; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * 파괴 연출이 클라이언트에 도착할 때까지 액터를 남겨 두는 시간(초).
     *
     * 0 으로 두면 서버가 즉시 Destroy 해서 액터가 복제보다 먼저 사라진다.
     * 그러면 클라이언트에서는 RepNotify 도 BP 연출도 실행되지 않고
     * 물건이 소리 없이 증발한다. 지연 시간 동안에는 이미 숨겨져 있으므로
     * 플레이어 눈에는 파괴 즉시 사라진 것으로 보인다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Durability",
        meta = (ClampMin = "0.0"))
    float BreakDestroyDelay = 0.25f;

    /**
     * 충격이 하나 쌓였을 때 호출된다. 금 가는 단계 연출용.
     * BP 는 여기서 머티리얼·메시·사운드만 바꾼다. 판정은 이미 C++ 에서 끝났다.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Loot|Durability")
    void OnDamageAccumulated(int32 NewImpactCount, int32 MaxCount);

    /**
     * 파괴됐을 때 호출된다. 파편·사운드 연출용.
     * 이 시점에 노획물 메시는 이미 숨겨져 있다. 파편은 별도 액터/나이아가라로 스폰할 것.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Loot|Durability")
    void OnBroken();

private:
    /** ALootBase 의 확정 충격 구독 핸들러 (서버에서만 불린다) */
    void HandleLootImpact(const FLootImpactEvent& Event);

    /** 파괴 처리 (서버 전용) */
    void Break(const FLootImpactEvent& CausingEvent);

    /** 숨김·충돌 차단·BP 연출. 모든 머신에서 실행된다 */
    void ApplyBrokenState();

    /** BreakDestroyDelay 뒤에 불린다 (서버 전용) */
    void DestroyOwnerLoot();

    UFUNCTION()
    void OnRep_ImpactCount();

    UFUNCTION()
    void OnRep_IsBroken();

    /** 누적 충격 횟수. 클라이언트는 이 값으로 금 간 연출 단계를 맞춘다 */
    UPROPERTY(ReplicatedUsing = OnRep_ImpactCount, VisibleInstanceOnly, Category = "Loot|Durability")
    int32 ImpactCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_IsBroken, VisibleInstanceOnly, Category = "Loot|Durability")
    bool bIsBroken = false;

    /** 소유 노획물. UPROPERTY 가 없으면 GC 가 회수한 뒤 엉뚱한 곳에서 크래시한다 */
    UPROPERTY()
    TObjectPtr<ALootBase> OwnerLoot;

    /** EndPlay 에서 구독을 해제하기 위한 핸들 */
    FDelegateHandle ImpactDelegateHandle;

    FTimerHandle DestroyTimerHandle;
};
