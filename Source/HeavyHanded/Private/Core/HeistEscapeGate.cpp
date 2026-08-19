#include "Core/HeistEscapeGate.h"

namespace HeistEscapeGate
{
	int32 GetSurvivorNum(const FHeistEscapeConditions& Conditions)
	{
		// 다운자가 접속자보다 많을 수는 없지만, 두 값이 서로 다른 시점에 세어졌다면
		// 한 프레임 어긋날 수 있다. 그때 음수 생존자가 나오면 아래 판정이 조용히 뒤집힌다.
		return FMath::Max(0, Conditions.NumActivePlayers - Conditions.NumDownedPlayers);
	}

	bool HasEveryoneEscaped(const FHeistEscapeConditions& Conditions)
	{
		const int32 SurvivorNum = GetSurvivorNum(Conditions);

		// 아무도 서 있지 않다. 전원이 쓰러진 것을 "전원 탈출" 로 읽으면 안 된다 —
		// 0명 중 0명이 탔다는 것은 참이지만, 그 참이 뜻하는 바가 탈출이 아니다.
		// 이 판은 도주 시간이 다 되어 전원 체포로 끝나야 한다.
		if (SurvivorNum <= 0)
		{
			return false;
		}

		// 초과는 있을 수 없지만 >= 로 둔다. == 로 두면 두 값이 한 프레임 어긋났을 때
		// 판정이 영영 성립하지 않아 판이 안 끝난다 — 틀리는 방향을 고르는 것이다.
		return Conditions.NumBoardedSurvivors >= SurvivorNum;
	}
}
