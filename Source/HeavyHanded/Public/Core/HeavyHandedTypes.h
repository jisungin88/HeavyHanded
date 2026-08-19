#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Chaos/ChaosEngineInterface.h"
#include "HeavyHandedTypes.generated.h"

// TWeakObjectPtr 전방 선언 (헤더에서는 #include 하지 않는다 — 컨벤션)
class AActor;
class APawn;

/** 노획물 무게 등급 — 운반 가능 여부와 이동 페널티의 기준 */
UENUM(BlueprintType)
enum class EWeightClass : uint8
{
    Light   UMETA(DisplayName = "Light"),   // 한 손, 페널티 없음
    Normal  UMETA(DisplayName = "Normal"),   // 1인 운반, 약한 페널티
    Heavy   UMETA(DisplayName = "Heavy")  // 2인 필수 (기획서 5장)
};

/**
 * 충돌 원인 구분 — FLootImpactEvent 에 실려 소음 파트로 전달된다.
 * "무엇 때문에 부딪혔는가"만 알린다. 그게 얼마나 시끄러운지는 소음 파트가 해석한다.
 */
UENUM(BlueprintType)
enum class ELootImpactCause : uint8
{
    Drop      UMETA(DisplayName = "Drop"),      // 놓기
    Throw     UMETA(DisplayName = "Throw"),     // 던지기
    Collision UMETA(DisplayName = "Collision"), // 일반 충돌 (낙하·튕김·구름)
    Break     UMETA(DisplayName = "Break")      // 파괴
};

/**
 * 노획물의 물리 데이터.
 * 값은 블루프린트에서 지정하고, 로직은 C++에서만 소비한다. (컨벤션 4-3)
 */
USTRUCT(BlueprintType)
struct FLootPhysicsData
{
    GENERATED_BODY()

    /** 무게 등급 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Physics")
    EWeightClass WeightClass = EWeightClass::Normal;

    /** 물리 질량(kg). 던지기 충격량과 낙하 소음 계산에 사용 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Physics",
        meta = (ClampMin = "0.1"))
    float MassKg = 10.f;

    /** 들기 위해 필요한 최소 인원 (기획서: 중량형 2) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Carry",
        meta = (ClampMin = "1", ClampMax = "2"))
    int32 RequiredCarriers = 1;

    /** 소지 중 이동 속도 배율 (기획서: 중량형 1인 시 0.3) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Carry",
        meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float CarrySpeedMultiplier = 1.f;

    /** 소지 중 점프 허용 여부 (기획서: 중량형 점프 불가) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Carry")
    bool bAllowJumpWhileCarried = true;

    /**
     * 놓을 때 앞으로 살짝 던지는 속도(cm/s).
     *
     * 제자리에서 툭 떨어뜨리면 물건이 발밑에 박혀 다시 집기도 번거롭고,
     * 버렸다는 느낌도 안 난다. 보는 방향으로 약하게 밀어 준다.
     * 던지기(ThrowSpeed)와는 자릿수가 다르다 — 이건 '버리기'지 '던지기'가 아니다.
     * 0 이면 제자리에서 떨어진다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Drop",
        meta = (ClampMin = "0.0"))
    float DropSpeed = 200.f;

    /** 버릴 때 섞을 위쪽 성분의 비율. 살짝 떠서 굴러가야 툭 놓은 느낌이 난다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Drop",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DropUpwardRatio = 0.35f;

    /** 버릴 때 부여할 회전 속도(도/초). 던지기보다 약하게 준다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Drop",
        meta = (ClampMin = "0.0"))
    float DropSpinSpeed = 90.f;

    /** 던질 수 있는가. 중량형처럼 놓기만 되는 물건은 false 로 둔다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Throw")
    bool bAllowThrow = true;

    /**
     * 던졌을 때의 목표 초기 속도(cm/s).
     * 임펄스는 질량을 곱해 만들기 때문에, 무게가 달라도 이 속도 그대로 나간다.
     * 무거운 물건이 덜 날아가는 것은 여기 값을 낮게 잡아 표현한다. (행동이 아니라 데이터로)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Throw",
        meta = (ClampMin = "0.0"))
    float ThrowSpeed = 900.f;

    /**
     * 조준 방향에 섞을 위쪽 성분의 비율. 포물선을 만든다.
     * 0 이면 조준한 그대로 직선으로 나가 바닥에 바로 박힌다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Throw",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ThrowUpwardRatio = 0.25f;

    /**
     * 던질 때 운반자의 이동 속도를 얼마나 더할지 (0 = 안 더함, 1 = 그대로 더함).
     *
     * 물리적으로는 1 이 맞다. 손에 든 물건은 나와 같이 움직이고 있으니까.
     * 그런데 1 로 두면 조준이 불가능해진다. 이동 속도가 ThrowSpeed 와 비슷하면
     * 뒷걸음질만 쳐도 물건이 뒤로 날아가고, 좌우로 한 발짝에 궤적이 크게 꺾인다.
     * 밴에 던져 넣기처럼 정확도가 필요한 동작이 운에 좌우된다.
     *
     * 그래서 기본은 0 이다. 던지는 순간의 발밑 상태와 무관하게 조준한 대로 나간다.
     * 플레이 테스트에서 밋밋하면 0.2~0.3 정도로 조금씩 올린다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Throw",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CarrierVelocityInfluence = 0.f;

    /** 던질 때 부여할 회전 속도(도/초). 0 이면 회전 없이 날아가 던진 티가 안 난다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Throw",
        meta = (ClampMin = "0.0"))
    float ThrowSpinSpeed = 180.f;

    /**
     * 던지기 직전 조준 방향으로 밀어내는 거리(cm).
     * 손 소켓은 던진 사람의 캡슐과 겹쳐 있어서, 그대로 물리를 켜면
     * 물리 엔진이 겹침을 해소하느라 자기가 던진 물건에 튕긴다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Throw",
        meta = (ClampMin = "0.0"))
    float ThrowClearance = 20.f;

    /**
     * 이 값 미만의 충격은 FLootImpactEvent 를 방송하지 않는다.
     * 미세 진동·재접촉을 걸러 소음 파트의 경계도가 순식간에 치솟는 것을 막는다.
     * (물리 낙하 1회에 OnHit 은 5~15회 발생한다)
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Impact",
        meta = (ClampMin = "0.0"))
    float ImpactReportThreshold = 200.f;

    // 파손 수치(DamageImpulseThreshold / MaxImpactCount)는 여기 없다.
    // ULootDurabilityComponent 만 읽는 값이라 파손형이 아닌 노획물까지 들고 다닐 이유가 없다.
    // FLootDurabilityData 로 옮겨 DT_LootDurability 에 행 이름으로 조인한다.
};

/**
 * 노획물 물리 충돌 이벤트. 물리·아이템 파트 → 소음 파트로 전달되는 데이터 단위.
 *
 * [원칙] 아이템은 '물리적 사실'만 알린다. 해석은 소음 파트가 한다.
 *   무엇이 / 얼마의 충격으로 / 무슨 재질에 / 왜 부딪혔는지만 담는다.
 *   그게 얼마나 시끄러운 소리인지는 여기서 판단하지 않는다.
 */
USTRUCT(BlueprintType)
struct FLootImpactEvent
{
    GENERATED_BODY()

    /** 충돌 지점 (월드 좌표). VFX·데칼 스폰 위치로도 쓴다 */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    FVector ImpactPoint = FVector::ZeroVector;

    /** 충돌 표면의 노멀. 데칼 방향·튕김 판단에 사용 */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    FVector ImpactNormal = FVector::ZeroVector;

    /** 충격량 크기. 소음 등급·카메라 셰이크 강도의 원천 값 */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    float ImpulseMagnitude = 0.f;

    /** 부딪힌 바닥·벽의 물리 재질 (Config 정의 7종) */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

    /** 충돌 원인 (놓기/던지기/일반충돌/파괴) */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    ELootImpactCause Cause = ELootImpactCause::Collision;

    /** 부딪힌 노획물 액터 */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    TWeakObjectPtr<AActor> LootActor = nullptr;

    /**
     * 부딪힌 상대. 바닥·벽·다른 노획물·플레이어 등.
     *
     * '무엇에 부딪혔는가'는 재질만으로는 부족하다. 같은 콘크리트라도 바닥에 떨어진 것과
     * 사람이 밀친 것은 다른 사건이다. 파손 컴포넌트는 이 값으로 사람 몸과의 접촉을 걸러내고,
     * 소음 파트도 필요하면 여기서 상대를 확인할 수 있다.
     * 파괴(Break)처럼 부딪힌 상대가 없는 사건에서는 비어 있다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    TWeakObjectPtr<AActor> HitActor = nullptr;

    /** 원인을 제공한 플레이어. 결과 화면 '최다 소음 유발자' 집계용 */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    TWeakObjectPtr<APawn> InstigatorPawn = nullptr;

    /** 서버 기준 발생 시각 */
    UPROPERTY(BlueprintReadOnly, Category = "Loot|Impact")
    float ServerTime = 0.f;
};

/**
 * 노획물 충돌 방송용 델리게이트.
 *
 * 다이내믹(BP)이 아닌 C++ 전용 멀티캐스트로 둔다.
 *   - 소음 파트는 서버에서 AddUObject 로 구독해 FLootImpactEvent 를 받는다.
 *   - BP로 열지 않아 누군가 BP에 소음 판정 로직을 짜는 것을 원천 차단한다. (컨벤션)
 * 실제 인스턴스는 방송 주체인 ALootBase 가 소유한다. (작업 순서 5단계)
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLootImpactSignature, const FLootImpactEvent&);