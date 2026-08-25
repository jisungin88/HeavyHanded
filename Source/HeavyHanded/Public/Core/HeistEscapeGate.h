#pragma once

#include "CoreMinimal.h"

/**
 * 탈출 판정에 필요한 사실 전부. 여기 없는 것은 판정에 영향을 주지 않는다.
 * 페이즈가 없는 것은 의도다 — 넣는 순간 태그를 알아야 해서 순수 함수가 아니게 된다.
 */
struct FHeistEscapeConditions
{
	/**
	 * 이 판에 남아 있는 플레이어 수. 끊긴 사람은 빠진다.
	 * 체포된 사람은 포함된다 — 체포는 도주 시간이 끝날 때 확정된다.
	 */
	int32 NumActivePlayers = 0;

	/** 그중 다운 상태인 인원. 다운자는 탈출 대상이 아니다 */
	int32 NumDownedPlayers = 0;

	/** 승차 명단에 있으면서 다운되지 않은 인원 */
	int32 NumBoardedSurvivors = 0;
};

/**
 * "생존자가 전부 밴에 탔는가" 판정. 월드를 모르는 순수 함수라 테스트가 직접 부른다.
 * **전원이 다운되면 생존자가 0명**인데, 그대로 물으면 0명 중 0명이 참이 되어
 * 바닥에 넷이 쓰러진 판이 탈출 성공으로 정산된다 — 그 경계 때문에 값으로 검증한다.
 */
namespace HeistEscapeGate
{
	HEAVYHANDED_API bool HasEveryoneEscaped(const FHeistEscapeConditions& Conditions);

	/** 생존자 수(= 접속자 - 다운자). 음수가 되지 않게 잘라 준다 */
	HEAVYHANDED_API int32 GetSurvivorNum(const FHeistEscapeConditions& Conditions);
}
