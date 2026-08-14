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
}
