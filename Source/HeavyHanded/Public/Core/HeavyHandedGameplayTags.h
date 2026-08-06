#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * HeavyHanded 게임플레이 태그 선언.
 *
 * 태그를 문자열로 조회하면 오타를 컴파일러가 잡지 못하고 런타임에 조용히 실패한다.
 * 코드에서 쓰는 태그는 여기에 선언한다. 문자열 정의는 Config/DefaultGameplayTags.ini 에 있다.
 *
 * [경계] Noise.* 태그 6종은 소음 파트 소유로 이관되었다.
 *   물리·아이템 파트는 태그 대신 FLootImpactEvent(물리적 사실)만 방송한다.
 *   .ini 의 문자열 정의는 소음 파트가 쓰므로 남겨 둔다. (건드리지 않는다)
 *
 * 물리·아이템 파트 전용 태그가 필요해지면 이 네임스페이스에 추가한다.
 */
namespace HHTags
{
}
