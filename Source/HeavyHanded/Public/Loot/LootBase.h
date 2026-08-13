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
     * 예측 궤적을 그린다. 기본값은 한 프레임만 그리므로 조준 중 매 프레임 호출하면 된다.
     *
     * [경계] 언제 그릴지(조준 버튼을 누르고 있는 동안)는 플레이어 파트가 정한다.
     *   아이템은 '자기가 어떻게 날아갈지'만 그린다.
     *   실제로 던질 때와 같은 ComputeThrowVelocity 를 쓰므로 표시와 결과가 어긋나지 않는다.
     *
     * 지금은 디버그 선으로 그린다. 최종 연출(스플라인 메시·나이아가라 리본)은 나중에 교체한다.
     */
    void ShowThrowTrajectory(const FVector& AimDirection, float Duration = -1.f);

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

    /**
     * 물리 충돌이 아닌 사유(파괴 등)로 확정 충격을 즉시 방송한다. (서버 전용)
     *
     * 게이팅을 거치지 않는다. 게이팅은 '물리 낙하 1회가 OnHit 5~15회로 쪼개지는 것'을
     * 되묶기 위한 장치인데, 파괴는 애초에 한 번만 일어나는 사건이라 되묶을 것이 없다.
     */
    void ReportImpact(ELootImpactCause InCause, float ImpulseMagnitude,
        const FVector& ImpactPoint, APawn* InInstigator);

    /** 파손 컴포넌트 등이 같은 스위치로 디버그를 켜고 끄기 위해 연다 */
    bool IsImpactDebugEnabled() const { return bShowImpactDebug; }

    /**
     * [임시] 잡기/놓기 토글. 0번 로컬 플레이어 기준.
     *
     * 집기 입력·트레이스는 플레이어 파트 담당이라 아직 호출해 줄 것이 없다.
     * DebugGrabRange 안에 있을 때만 반응하므로, 여러 개를 깔아 둬도
     * 가까이 간 하나만 잡힌다. 플레이어 파트가 연결되면 지운다.
     */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Loot|Debug")
    void Debug_ToggleGrabByLocalPlayer();

    /**
     * [임시] 조준 시작. T 를 누르고 있는 동안 궤적이 매 프레임 갱신된다.
     * 실제 게임에서는 플레이어 파트가 조준 입력을 받아 ShowThrowTrajectory 를 호출한다.
     */
    UFUNCTION(BlueprintCallable, Category = "Loot|Debug")
    void Debug_BeginThrowAim();

    /**
     * [임시] 조준을 끝내고 던진다. T 를 뗀 순간 호출된다.
     * 보고 있던 궤적을 6초간 남겨서 실제 경로와 겹치는지 비교할 수 있게 한다.
     */
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Loot|Debug")
    void Debug_ThrowForward();

protected:
    virtual void BeginPlay() override;

    /** 평소에는 꺼져 있고 조준 중에만 켜진다 (궤적 갱신용) */
    virtual void Tick(float DeltaSeconds) override;

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
     * 놓을 때 운반자 몸에서 앞으로 띄우는 여유(cm).
     *
     * 두 형상의 반경을 더한 값에 이만큼 더 벌린다. 딱 붙여 놓으면 놓자마자
     * 한 발짝만 움직여도 자기가 놓은 물건에 닿는다.
     * 파손은 이 거리로 막는 것이 아니다 — 그건 bIgnorePawnImpacts 가 담당한다.
     * 여기서는 놓은 물건이 발에 차이지 않을 정도만 확보한다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Carry",
        meta = (ClampMin = "0.0"))
    float ReleaseForwardClearance = 30.f;


    /**
     * 같은 대상에 대한 재충돌을 이 시간 동안 무시한다.
     * 임계값만으로는 부족하다 — 세게 떨어지면 강한 충격이 연달아 여러 번 잡힌다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Impact",
        meta = (ClampMin = "0.0"))
    float ImpactDebounceSeconds = 0.3f;

    /**
     * 충돌 게이팅이 실제로 도는지 화면에 표시한다. (테스트용, 기본 꺼짐)
     *
     * 확정된 충격뿐 아니라 '기각된' 충격도 사유와 함께 찍는다.
     * 낙하 1회에 OnHit 이 몇 번 오고 그중 몇 개가 통과하는지를 눈으로 봐야
     * 임계값(200/3000)과 디바운스(0.3초)가 적당한지 판단할 수 있다.
     *
     * 판정은 서버에서만 돌기 때문에 표시도 서버(호스트 창)에만 나온다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Debug")
    bool bShowImpactDebug = false;

    /**
     * [임시] G = 잡기/놓기, T = 던지기 키를 이 액터에 연결한다.
     *
     * 잡기 입력이 아직 없어서(플레이어 파트 담당) 손으로 눌러 볼 수단이 필요하다.
     * 판정이 서버 전용이라 호스트 창에서만 연결된다. 클라이언트 창에서는 눌러도 반응이 없다.
     * 플레이어 파트가 연결되면 이 스위치와 Debug_ 함수들을 통째로 지운다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Debug")
    bool bDebugEnableTestKeys = false;

    /** [임시] 이 거리 안에 있을 때만 G 키에 반응한다 (cm) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Debug",
        meta = (ClampMin = "0.0"))
    float DebugGrabRange = 400.f;

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

    /**
     * 물리를 켜기 전에 운반자 몸 밖으로 빼낸다. (서버 전용)
     * 겹친 채로 켜면 물리 엔진이 침투를 밀어내며 만든 임펄스가 그대로 충돌로 잡히고,
     * 버린 물건이 엉뚱한 방향으로 튄다.
     */
    void ResolveReleaseOverlap(const APawn* Carrier);

    /**
     * 버릴 때 보는 방향으로 약하게 밀어 준다. (서버 전용)
     * 던지기와 같은 경로를 쓰지 않는다 — 버리기는 조준도 궤적 예측도 없고 값도 훨씬 작다.
     */
    void ApplyDropImpulse(const APawn* Carrier);

    /** 로그 + 화면 메시지 + 충돌 지점 구. bShowImpactDebug 가 꺼져 있으면 아무것도 하지 않는다 */
    void ShowImpactDebug(const FString& Message, const FColor& Color, const FVector& Location) const;

    /** [임시] G/T 키를 이 액터에 연결한다. BeginPlay 에서 부른다 */
    void Debug_SetupTestKeys();

    /** [임시] T 를 누르고 있는 동안 참. 이때만 틱이 돈다 */
    bool bDebugAiming = false;

    /**
     * [임시] 조준 방향을 구한다. 궤적 표시와 실제 던지기가 같은 값을 써야 하므로 한 곳에 둔다.
     *
     * 시선 방향을 그대로 쓰지 않는다. 발사점(손)이 화면 중앙에서 벗어나 있으면
     * 시선 방향으로 던졌을 때 조준점 옆으로 날아가고, 멀수록 오차가 커진다.
     * 카메라에서 트레이스해 조준점을 먼저 찾고, 발사점에서 그 지점을 향하게 한다.
     *
     * 플레이어 파트가 OnThrown 에 넘길 AimDirection 도 이렇게 계산해야 한다.
     */
    FVector Debug_ComputeAimDirection() const;

    /** OnHit 콜백이 온 총 횟수. 확정 횟수와 비교해 게이팅 효과를 본다 */
    int32 DebugRawHitCount = 0;

    /** 게이팅을 통과해 방송된 횟수 */
    int32 DebugConfirmedCount = 0;

    /** 대상별 마지막 확정 충격 시각. 키가 죽으면 정리된다 */
    TMap<TWeakObjectPtr<const AActor>, float> RecentImpactTimes;

    /** 상호 무시를 걸어 둔 운반자. 운반자가 바뀔 때 해제 대상을 놓치지 않기 위해 따로 들고 있는다 */
    TWeakObjectPtr<APawn> MoveIgnoredCarrier;

    /** SetPendingImpactCause 로 예약된 원인. 확정 충격 1회에 소비된다 */
    ELootImpactCause PendingImpactCause = ELootImpactCause::Collision;

    /** 예약된 원인 제공자 */
    TWeakObjectPtr<APawn> PendingInstigatorPawn;
};
