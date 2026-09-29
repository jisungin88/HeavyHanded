#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HazardBase.generated.h"

class ABaseCharacter;
class USoundBase;

/**
 * 함정·감지 장치 등 환경 방해 요소 9종이 공유하는 추상 베이스.
 * (기획서 6장 — "동일 베이스 클래스, 장소별로 메시·파라미터만 변경")
 *
 * [무엇을 올렸는가]
 *   9개 클래스에 똑같이 반복되던 패턴 중 안전하게 뽑을 수 있는 세 가지만 올렸다.
 *   ① ABaseCharacter 캐스팅 + AGuardCharacter 배제 + State.ShadowStep 무시 판정
 *   ② 재발동 디바운스(재무장과 다르다 — 오버랩 경계에서 스치는 재진입만 걸러낸다)
 *   ③ 데디케이티드 서버 가드를 포함한 사운드 재생
 *
 * [무엇을 안 올렸는가]
 *   서버 권위 체크(`HasAuthority()`)는 CLAUDE.md 3절이 이미 "AActor 는
 *   AActor::HasAuthority() 를 직접 쓴다" 고 정해 뒀고, 클래스마다 `!bArmed`
 *   `bDisabled` 같은 다른 조건과 한 줄로 묶여 호출되는 경우가 많아 억지로
 *   감싸면 그 조합을 못 쓰게 된다 — 그래서 각 클래스가 그대로 직접 쓴다.
 *   트리거 볼륨 생성, 함정 고유 효과, Multicast RPC 선언은 손대지 않는다
 *   (함정마다 크기·의미가 달라 억지로 공통화하면 오히려 혼란을 준다).
 *
 * [MinRetriggerInterval 을 여기 두지 않은 이유]
 *   클래스마다 기본값이 다르다(0.5~3초). 각자 자기 UPROPERTY 를 그대로 유지하고
 *   ShouldRetrigger() 호출부에 그 값을 넘기기만 한다 — 재발동 개념이 없는
 *   클래스(ABreakableWall 등)의 디테일 패널에 안 쓰는 값이 뜨는 것을 막는다.
 *
 * [ASecurityCamera 는 ShouldRetrigger() 를 쓰지 않는다]
 *   다른 세 클래스는 "판정이 이미 끝난 대상" 을 확인하자마자 곧바로 재발동 체크를
 *   하지만, 카메라는 시야 안의 모든 캐릭터를 훑기 *전에* 체크해야 해서 순서가
 *   다르다 — 여기서 억지로 맞추면 매 CheckDetection 호출(0.2초 간격)마다 재발동
 *   시각이 갱신돼 디바운스가 사실상 무력화된다. 그래서 카메라는 자기 필드를
 *   그대로 갖고 자기 로직으로 판정한다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API AHazardBase : public AActor
{
	GENERATED_BODY()

protected:
	/**
	 * OtherActor 가 이 함정의 판정 대상이 될 수 있는지 확인한다.
	 * ABaseCharacter 가 아니면(AGuardCharacter 포함) false — 경비가 자기 구역
	 * 함정에 스스로 걸리지 않는 이유가 이것이다.
	 *
	 * @param bCheckShadowStep true(기본)면 State.ShadowStep 태그 보유 시에도 false 를 반환한다.
	 *        AForceMovementZone::OnZoneEndOverlap 처럼 "나가는" 판정엔 false 로 넘겨 끌 수 있다.
	 */
	bool IsValidHazardTarget(AActor* OtherActor, ABaseCharacter*& OutTarget, bool bCheckShadowStep = true) const;

	/**
	 * Interval 초 이내에 다시 호출됐으면 false. 재무장(Rearm) 개념이 아니라
	 * 오버랩 경계에서 미세하게 들락거리는 것(스침)만 걸러내는 디바운스다.
	 * 통과하면 내부적으로 마지막 발동 시각을 지금으로 갱신한다.
	 */
	bool ShouldRetrigger(float Interval);

	/** 데디케이티드 서버에서는 아무것도 안 하고, 그 외에는 GetActorLocation() 에서 재생한다. */
	void PlayHazardSound(USoundBase* Sound) const;

private:
	/** 마지막으로 ShouldRetrigger() 가 통과시킨 시각(GetWorld()->GetTimeSeconds() 기준). 복제하지 않는다 — 서버 전용 판정 */
	float LastTriggerTime = -1.f;
};
