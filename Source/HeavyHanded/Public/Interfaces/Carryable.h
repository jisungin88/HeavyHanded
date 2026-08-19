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
	 * 지금 던진다면 어느 방향으로 나가야 하는가. 들려 있지 않으면 영벡터.
	 *
	 * 운반자의 시선을 그대로 쓰면 안 된다. 발사점(손)이 화면 중앙에서 벗어나 있어서
	 * 시선 방향으로 던지면 조준점 옆으로 날아가고, 목표가 멀수록 오차가 커진다.
	 * 보정에 필요한 발사점은 물건만 알기 때문에 이 계산이 아이템 쪽에 있다.
	 *
	 * 플레이어 파트는 '언제 던지는가' 만 정하고 이 값을 그대로 OnThrown 에 넘긴다.
	 * 인터페이스에 두는 이유는 플레이어 파트가 ALootBase 를 include 하지 않게 하기 위해서다.
	 */
	virtual FVector ComputeThrowAimDirection() const = 0;

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
