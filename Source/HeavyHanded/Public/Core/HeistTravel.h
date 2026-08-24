#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"   // FSoftObjectPath 를 값으로 주고받는다

/**
 * 레벨 사이를 ServerTravel 할 때 어떤 URL 을 쓰는가를 만드는 곳.
 *   은신처 → 작업 장소(URunProgressSubsystem::TryDepartToSite)와
 *   작업 장소 → 은신처(AHeistGameMode::FinishMatch) 양쪽이 같이 쓴다.
 *
 * [왜 한 줄짜리를 떼어냈는가]
 *   이 문자열이 틀려도 **아무 일도 일어나지 않은 것처럼 보인다.** 맵은 정상적으로 열리고
 *   전원이 저택에 도착한다. 다만 ?ExpectedPlayers 가 빠지거나 이름이 어긋나면
 *   AHeistGameMode 가 인원을 모르는 채로 시작해서 PlayerJoinQuietSeconds 폴백으로 떨어진다 —
 *   그 경로의 알려진 증상이 "가끔 한 명 두고 출발하더라" 다 (HeistStartGate).
 *
 *   즉 실패가 레벨 로딩이 아니라 **며칠 뒤의 간헐적 버그**로 나타난다. 옵션 이름은
 *   AHeistGameMode::ResolveExpectedPlayers 의 GetIntOption 문자열과 한 글자도 다르면 안 되는데,
 *   그 두 곳은 서로를 모른다. 테스트가 그 짝을 못박는다.
 *
 * [월드를 모르는 순수 함수다] HeistStartGate · HeistEntryGate · HeistEscapeGate 와 같은 형태다.
 *   부수효과(설정 조회 · 실제 ServerTravel · 로그)는 호출부인 URunProgressSubsystem 이 갖는다.
 */
namespace HeistTravel
{
	/**
	 * ServerTravel 에 넘길 URL 을 만든다.
	 *
	 * @param LevelPath       작업 레벨. UHeistSettings::GetSiteLevel 이 돌려준 것
	 * @param ExpectedPlayers 몇 명을 기다릴 것인가. 0 이하면 옵션을 붙이지 않는다
	 *
	 * @return 떠날 URL. **LevelPath 가 비어 있으면 빈 문자열이고, 그때는 떠나면 안 된다.**
	 *
	 * [왜 실패를 빈 문자열로 돌려주는가]
	 *   "모르겠으면 기본 맵" 같은 폴백이 여기 있으면 안 된다. 장소 매핑이 빠진 것을
	 *   엉뚱한 레벨이 열리는 것으로 알게 되고, 그때는 이미 전원이 그리로 끌려간 뒤다.
	 *   AHeistEntryPoint::TryGetVanTransform 과 같은 판단이다 — 실패는 값으로 드러낸다.
	 *
	 * [ExpectedPlayers 를 0 으로 두어도 되는 경우]
	 *   PIE 에서 레벨을 직접 열어 보는 것처럼 인원을 모를 때다. 실제 매치에서는 반드시
	 *   채운다 — 안 채우면 위 주석의 폴백 경로로 떨어진다.
	 */
	HEAVYHANDED_API FString BuildTravelURL(const FSoftObjectPath& LevelPath, int32 ExpectedPlayers);
}
