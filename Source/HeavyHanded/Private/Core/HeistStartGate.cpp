#include "Core/HeistStartGate.h"

namespace HeistStartGate
{
	EHeistStartDecision Evaluate(const FHeistStartConditions& Conditions)
	{
		// 0. 상한. 한 명이 로딩에서 영영 안 돌아와도 나머지가 붙잡혀 있으면 안 된다.
		//    다른 어떤 조건보다 먼저 본다 — 이 안전망이 다른 규칙에 막히면 안전망이 아니다
		if (Conditions.SecondsUntilDeadline <= 0.f)
		{
			return EHeistStartDecision::TimedOut;
		}

		// 1. 접속은 했는데 아직 레벨 로딩이 안 끝난 사람이 있다.
		//    인원이 다 찼는지와 무관하게 기다린다 — 로딩 화면에 준비 시간을 태울 수는 없다.
		//    올 인원을 몰라도 이것만은 막을 수 있다는 점이 중요하다
		if (Conditions.NumTravellingPlayers > 0)
		{
			return EHeistStartDecision::Wait;
		}

		// 2. 올 인원을 안다 — 다 찰 때까지 기다린다. 이것이 정상 경로다
		if (Conditions.ExpectedPlayers > 0)
		{
			return (Conditions.NumPlayers >= Conditions.ExpectedPlayers)
				? EHeistStartDecision::Ready
				: EHeistStartDecision::Wait;
		}

		// 3. 인원을 모른다. 마지막 접속 후 조용해지기를 기다리는 수밖에 없다.
		//
		//    [한계] 3명이 들어온 뒤 4번째가 QuietSeconds 보다 늦으면 두고 출발한다.
		//      몇 명이 올지 모르는 채로는 원리적으로 막을 수 없다.
		//      실제 매치에서는 ExpectedPlayers 가 채워져 여기까지 오지 않아야 한다.
		return (Conditions.SecondsSinceLastLogin >= Conditions.QuietSeconds)
			? EHeistStartDecision::Ready
			: EHeistStartDecision::Wait;
	}
}
