#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * HeavyHanded 게임플레이 태그 네이티브 선언.
 *
 * 태그를 문자열로 조회하면 오타를 컴파일러가 잡지 못하고 런타임에 조용히 실패한다.
 * C++ 에서 참조하는 태그는 여기에 선언하고, 문자열 정의는 Config/Tags/<시스템>.ini 에
 * 그대로 둔다 — 둘은 짝이다.
 *
 * [범위] 지금 C++ 이 실제로 참조하는 태그만 선언한다.
 *   .ini 의 태그를 미리 다 옮겨오지 않는다 — 쓰지 않는 선언은 또 하나의 목록이 되고,
 *   .ini 와 어긋나도 아무도 모른다.
 *
 * [소유] 각 태그는 해당 .ini 파일 담당자 소유다 (문서 06 협업 규칙).
 *   남의 시스템 태그를 여기 추가할 때는 담당자에게 먼저 알린다.
 *
 * [경계] 물리·아이템 파트는 태그 대신 FLootImpactEvent(물리적 사실)만 방송한다.
 *   그것이 얼마나 시끄러운지는 소음 파트가 해석한다.
 */
namespace HHTags
{
	// ── 소음 (지성인 / Config/Tags/Noise.ini) ──

	/** 물건 충돌. UNoiseEmitterComponent 가 에디터 미지정 시 쓰는 기본값 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Noise_Loot_Impact);

	/** 파손형 노획물이 깨질 때 나가는 태그 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Noise_Loot_Break);

	// ── 노획물 (김민준 / Config/Tags/Loot.ini) ──

	/**
	 * 특성 태그의 부모. "집을 수 있는 노획물인가" 판정이 이 하나로 끝난다.
	 *
	 * FGameplayTagContainer::HasTag 는 부모 매칭이라 Loot.Type.Heavy 같은 하위 태그가
	 * 전부 걸린다. 플레이어 파트의 GAB_Interact 가 이 방식으로 집기 대상을 고른다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type);

	/** 중량형 — 2인이어야 제 속도가 난다. 던지기 불가 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type_Heavy);

	/** 파손형 — ULootDurabilityComponent 가 붙어 있으면 자동으로 달린다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type_Fragile);

	/** 불안정형 — ULootStabilityComponent 가 붙어 있으면 자동으로 달린다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type_Unstable);

	// 상태 태그. 특성과 달리 런타임에 붙었다 떨어진다.

	/** 배치 상태 — 아무도 안 들었고 아직 옮겨진 적 없다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Idle);

	/** 1인 운반 중 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Carried);

	/** 바닥에 놓임. 환경 파트의 압력판이 이 태그로 판정한다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Dropped);

	/** 파괴됨 — 가치 0 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Broken);

	// Loot.State.Spilled 는 아직 선언하지 않는다.
	//   상태 태그는 배타적인데(하나만 유효), 유출은 들고 가는 도중에도 일어나므로
	//   Carried 를 밀어내 버린다. 파괴는 액터가 곧 사라져서 문제가 없지만 유출은 다르다.
	//   "샌 적이 있다" 는 사실이 필요해지면 상태가 아닌 별도 컨테이너로 붙일 것.
	//   지금은 ULootStabilityComponent::GetSpillCount() 와 ALootBase::IsValueLost() 로 읽는다.
}
