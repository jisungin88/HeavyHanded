#include "Core/HeistOutcome.h"

namespace HeistOutcome
{
	EHeistOutcome Evaluate(bool bTargetReached, bool bEveryoneEscaped)
	{
		if (bTargetReached && bEveryoneEscaped)
		{
			return EHeistOutcome::Success;
		}

		// 둘 중 하나만 된 경우는 성격이 다르지만 같은 등급이다.
		// 돈을 못 채우고 무사히 나온 판과, 돈은 챙겼지만 사람을 두고 온 판을 굳이 갈라
		// 등급을 넷으로 만들면 결과 화면이 설명해야 할 것만 늘어난다 —
		// 무엇이 모자랐는지는 등급이 아니라 적재액과 체포 명단이 말해 준다.
		if (bTargetReached || bEveryoneEscaped)
		{
			return EHeistOutcome::Partial;
		}

		return EHeistOutcome::Failure;
	}

	const TCHAR* ToString(EHeistOutcome Outcome)
	{
		switch (Outcome)
		{
		case EHeistOutcome::Success: return TEXT("성공");
		case EHeistOutcome::Partial: return TEXT("부분 성공");
		case EHeistOutcome::Failure: return TEXT("실패");
		}

		return TEXT("알 수 없음");
	}
}
