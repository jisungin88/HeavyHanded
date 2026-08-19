#include "Core/HeistOutcome.h"

namespace HeistOutcome
{
	EHeistOutcome Evaluate(bool bTargetReached, bool bAnyoneEscaped)
	{
		// 전멸은 목표를 채웠어도 실패다 (기획서 2장 — "목표 금액 미달 또는 플레이어 전멸").
		//
		// 두 조건이 대칭이 아닌 곳이 여기다. 금고를 다 털었어도 아무도 밴에 못 탔다면
		// 그 돈은 현장에 남는다 — 등급이 되지 않고 정산도 없다.
		if (!bAnyoneEscaped)
		{
			return EHeistOutcome::Failure;
		}

		// 여기부터는 최소 1인이 밴에 탔다. 남은 것은 목표 금액뿐이다.
		//
		// 몇 명을 두고 왔는지는 등급에 반영하지 않는다. 그 대가는 체포(다음 작업 관전)로
		// 이미 치르고, 누가 남았는지는 등급이 아니라 결과 화면의 체포 명단이 말해 준다.
		return bTargetReached ? EHeistOutcome::Success : EHeistOutcome::Partial;
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
