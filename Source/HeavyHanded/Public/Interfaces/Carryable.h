#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/HeavyHandedTypes.h"
#include "Carryable.generated.h"

class APawn;
class UPrimitiveComponent;

UINTERFACE(MinimalAPI)
class UCarryable : public UInterface
{
    GENERATED_BODY()
};

/**
 * 플레이어가 들어 옮길 수 있는 대상.
 *
 * [파트 경계]
 *   플레이어 파트   : 입력, 트레이스, 소지 상태 관리, 이동 페널티 '적용'
 *   물리·아이템 파트 : 아래 판정과 데이터 '제공'
 *
 * 플레이어는 요청하고, 아이템이 허용/거부한다.
 * 판정 함수는 서버에서만 신뢰한다. (컨벤션 3-1)
 */
class HEAVYHANDED_API ICarryable
{
    GENERATED_BODY()

public:
    /** 무게 등급 */
    virtual EWeightClass GetWeightClass() const = 0;

    /** 필요한 최소 인원 (중량형 = 2) */
    virtual int32 GetRequiredCarriers() const = 0;

    /** 소지 중 이동 속도 배율 — 플레이어 파트가 이 값을 곱해서 적용한다 */
    virtual float GetCarrySpeedMultiplier() const = 0;

    /** 소지 중 점프 허용 여부 */
    virtual bool IsJumpAllowedWhileCarried() const = 0;

    /**
     * 이 폰이 지금 들 수 있는가.
     * 이미 다른 사람이 들고 있는지, 인원이 충족되는지 등을 검사한다.
     */
    virtual bool CanBeCarriedBy(const APawn* Requester) const = 0;

    /** 잡힘 처리 (서버 전용). 물리 OFF + Attach 는 구현체 책임 */
    virtual void OnGrabbed(APawn* Carrier) = 0;

    /** 놓임 처리 (서버 전용). 물리 ON + 소음 발행 */
    virtual void OnReleased(APawn* Carrier) = 0;

    /** 던질 수 있는가. 중량형처럼 놓기만 되는 물건이 있다 */
    virtual bool CanBeThrown() const = 0;

    /**
     * 던짐 처리 (서버 전용). 놓기와 달리 초기 속도를 준다.
     *
     * AimDirection 은 플레이어 파트가 넘기는 조준 방향(정규화 전이어도 된다).
     * 세기·포물선·회전은 아이템이 자기 데이터로 결정한다 — 플레이어는 방향만 준다.
     * 던질 수 없는 물건이면 제자리에 놓는 것으로 처리한다.
     */
    virtual void OnThrown(APawn* Carrier, const FVector& AimDirection) = 0;

    /** 현재 이 노획물을 들고 있는 대표(리더). 없으면 nullptr */
    virtual APawn* GetPrimaryCarrier() const = 0;

    /** 물리 바디. 플레이어 파트가 Attach 대상으로 사용한다 */
    virtual UPrimitiveComponent* GetPhysicsRoot() const = 0;
};