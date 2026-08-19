#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Loot/LootTypes.h"              // FLootDurabilityData 를 값으로 보유 — 전방 선언 불가
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
 *   ImpactReportThreshold(200)   = 소음 파트에 알릴 최소 충격 — ALootBase 의 FLootPhysicsData
 *   DamageImpulseThreshold(3000) = 파손으로 칠 최소 충격 — 여기 Data
 *   살짝 부딪히는 소리는 나야 하지만 그게 파손까지 되면 안 된다.
 *
 *   실측(10kg): 150cm 낙하는 착지 6497 + 튕김 581 로 잡힌다.
 *   둘 다 방송되어 '쿵' 다음 '탁' 소리가 나지만, 파손은 착지 하나만 센다.
 *
 *   앞의 것이 액터에 있는 이유는 모든 노획물이 소리를 내기 때문이고,
 *   뒤의 것이 여기 있는 이유는 파손형만 깨지기 때문이다.
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
     * 설계 수치. DT_LootDurability 에서 노획물의 행 이름으로 찾아 채운다.
     *
     * [왜 컴포넌트가 직접 조회하나]
     *   이 값을 읽는 것은 여기뿐이다. 예전에는 FLootPhysicsData 에 섞여 있어서
     *   파손형이 아닌 노획물까지 전부 들고 다녔다. 무게·던지기 값 사이에 끼어 있어
     *   눈에 덜 띄었을 뿐 불안정형과 같은 문제였다.
     *
     * [BP 에서 고칠 수 있게 열어 두는 이유]
     *   표에 행이 없으면 여기 적힌 값이 그대로 쓰인다. 표에 안 올린 실험물용 폴백이다.
     *   행이 있으면 ResolveData 가 덮어쓰므로 여기 값은 무시된다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Durability")
    FLootDurabilityData Data;

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
     * 사람 몸(폰)에 닿은 충격은 파손으로 세지 않는다.
     *
     * [원칙] 물건이 상하는 건 바닥·벽·다른 물건에 부딪혔을 때다.
     *   사람이 밀치면 물건이 넘어지고, 넘어져서 바닥에 부딪히며 상한다.
     *   사람 몸에 스친 것 자체로 상하지는 않는다.
     *
     * 기술적으로도 이래야 한다. 캐릭터 캡슐은 키네마틱이라 물리 엔진이 질량을
     * 무한대처럼 다룬다. 걸어가다 살짝 닿기만 해도 임펄스가 바닥 낙하의 3~4배로 튄다.
     * (실측: 놓은 상자를 걸어서 밀었을 때 19010 — 3m 낙하가 9622인데도 그렇다)
     * 이 값을 거리로 막을 수는 없다. 플레이어는 언제든 물건 쪽으로 걸어갈 수 있다.
     *
     * 걸러내는 것은 파손 판정뿐이다. 충격 이벤트 자체는 그대로 방송되므로
     * 소음 파트는 사람이 물건을 밀친 사실을 여전히 받는다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Durability")
    bool bIgnorePawnImpacts = true;

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
    /**
     * DT_LootDurability 에서 자기 행을 찾아 Data 를 채운다. 한 번만 실제로 돈다.
     *
     * BeginPlay 뿐 아니라 OnRep_ImpactCount 앞에서도 부른다. 초기 복제는 BeginPlay 보다
     * 먼저 도착할 수 있는데, 그때 MaxImpactCount 가 기본값이면 클라이언트의 금 간 연출이
     * "2/3" 처럼 어긋난 단계로 뜬다.
     */
    void ResolveData();

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

    /** ResolveData 가 이미 돌았는가. 표 조회를 매번 반복하지 않기 위한 것이다 */
    bool bDataResolved = false;

    /** EndPlay 에서 구독을 해제하기 위한 핸들 */
    FDelegateHandle ImpactDelegateHandle;

    FTimerHandle DestroyTimerHandle;
};
