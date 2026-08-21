#include "Core/HeistEntryGate.h"

namespace HeistEntryGate
{
	FHeistEntryResolution Resolve(const TArray<FGameplayTag>& Available, const FGameplayTag& Selected,
		int32 DefaultIndex)
	{
		FHeistEntryResolution Result;

		// 진입점이 없는 레벨. 폴백할 대상 자체가 없으므로 호출부가 엔진 기본 스폰으로 넘겨야 한다
		if (Available.Num() == 0)
		{
			Result.Decision = EHeistEntryDecision::None;
			Result.Index = INDEX_NONE;

			return Result;
		}

		// 정확히 일치하는 것만 찾는다. 부모 매칭(MatchesTag)을 쓰지 않는 이유는
		// Entry.Mansion 하나를 고른 것이 Entry.Mansion.Front 를 고른 것과 같아지면,
		// 진입점이 늘어날 때 예전 선택이 조용히 다른 곳을 가리키기 때문이다
		if (Selected.IsValid())
		{
			const int32 Found = Available.IndexOfByKey(Selected);

			if (Found != INDEX_NONE)
			{
				Result.Decision = EHeistEntryDecision::Selected;
				Result.Index = Found;

				return Result;
			}
		}

		// 안 골랐거나(첫 판 · 치트로 바로 레벨을 연 경우), 고른 것이 이 레벨에 없다.
		// 둘을 같은 결과로 묶는 이유는 호출부가 할 일이 같기 때문이다 — 기본 진입점으로 시작하고 알린다.
		// 구분이 필요하면 Selected.IsValid() 로 호출부가 로그 문구만 가르면 된다
		Result.Decision = EHeistEntryDecision::Fallback;

		// 레벨이 지정한 기본 진입점이 있으면 그것. 범위를 벗어난 값은 지정이 없는 것으로 본다 —
		// 호출부가 인덱스를 잘못 계산했을 때 배열 밖을 짚는 것보다 첫 번째로 떨어지는 편이 낫다
		Result.Index = Available.IsValidIndex(DefaultIndex) ? DefaultIndex : 0;

		return Result;
	}

	const TCHAR* ToString(EHeistEntryDecision Decision)
	{
		switch (Decision)
		{
		case EHeistEntryDecision::Selected: return TEXT("Selected");
		case EHeistEntryDecision::Fallback: return TEXT("Fallback");
		case EHeistEntryDecision::None:     return TEXT("None");
		default:                            return TEXT("Unknown");
		}
	}
}
