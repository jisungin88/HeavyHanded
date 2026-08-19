#pragma once

#include "CoreMinimal.h"

/**
 * 탈출 판정에 필요한 사실 전부.
 *
 * 여기 없는 것은 판정에 영향을 주지 않는다 — 이 구조체가 곧 "무엇을 보고 정하는가" 의 목록이다.
 * 특히 페이즈가 없다: "지금 탈출을 받는 페이즈인가" 는 호출부가 판단한다.
 * 그걸 여기 넣으면 태그를 알아야 하고, 그 순간 이 판정은 순수하지 않게 된다.
 */
struct FHeistEscapeConditions
{
	/**
	 * 이 판에 남아 있는 플레이어 수. 접속이 끊긴 사람은 빠진다.
	 *
	 * 체포된 사람도 여기 포함된다 — 체포는 도주 시간이 끝날 때 확정되는 것이라
	 * 판정이 도는 동안에는 아직 아무도 체포 상태가 아니다.
	 */
	int32 NumActivePlayers = 0;

	/** 그중 다운 상태인 인원. 다운자는 탈출 대상이 아니다 */
	int32 NumDownedPlayers = 0;

	/** 승차 명단에 있으면서 다운되지 않은 인원 */
	int32 NumBoardedSurvivors = 0;
};

/**
 * "지금 판을 끝내도 되는가 — 생존자가 전부 밴에 탔는가" 판정.
 *
 * [왜 GameMode 밖으로 뺐는가] HeistStartGate 와 같은 이유다. 이 판정은 틀려도 크래시가 나지
 *   않고 "가끔 판이 일찍 끝나더라" 로만 드러난다. 재현하려면 넷이 모여 다운과 승차를 특정
 *   순서로 만들어야 해서 사람이 눈으로 잡을 수 없다.
 *
 *   특히 걸리는 경계가 하나 있다. **전원이 다운되면 생존자가 0명**인데, 그때 "생존자가 전부
 *   탔는가" 를 그대로 물으면 0명 중 0명이 탔으므로 참이 되어 판이 즉시 끝난다.
 *   바닥에 넷이 쓰러져 있는데 탈출 성공으로 정산되는 것이다. 값으로 검증할 수 있어야 한다.
 *
 *   Private/Tests/HeistStartGateTest.cpp 가 이 함수를 직접 부른다.
 */
namespace HeistEscapeGate
{
	HEAVYHANDED_API bool HasEveryoneEscaped(const FHeistEscapeConditions& Conditions);

	/** 생존자 수(= 접속자 - 다운자). 음수가 되지 않게 잘라 준다 */
	HEAVYHANDED_API int32 GetSurvivorNum(const FHeistEscapeConditions& Conditions);
}
