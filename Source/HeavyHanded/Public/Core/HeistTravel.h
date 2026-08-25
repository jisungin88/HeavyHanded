#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"   // FSoftObjectPath 를 값으로 주고받는다

/**
 * ServerTravel URL 을 만든다. 은신처 → 작업 장소와 그 반대가 같이 쓴다.
 * 이 문자열이 틀려도 맵은 멀쩡히 열린다 — ?ExpectedPlayers 이름이 어긋나면 인원을 모른 채
 * 폴백으로 떨어져 "가끔 한 명 두고 출발" 로만 드러난다. 테스트가 그 짝을 못박는다.
 */
namespace HeistTravel
{
	/**
	 * ServerTravel 에 넘길 URL 을 만든다. ExpectedPlayers 가 0 이하면 옵션을 붙이지 않는다.
	 * **LevelPath 가 비어 있으면 빈 문자열이고, 그때는 떠나면 안 된다** —
	 * "모르겠으면 기본 맵" 폴백을 두면 전원이 엉뚱한 레벨로 끌려간 뒤에야 알게 된다.
	 */
	HEAVYHANDED_API FString BuildTravelURL(const FSoftObjectPath& LevelPath, int32 ExpectedPlayers);
}
