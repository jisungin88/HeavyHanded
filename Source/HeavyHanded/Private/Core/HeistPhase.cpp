#include "Core/HeistPhase.h"

#include "Core/HeavyHandedGameplayTags.h"

namespace HeistPhase
{
	const TArray<FGameplayTag>& GetOrder()
	{
		/**
		 * 함수 지역 정적이다. 파일 스코프 전역으로 두면 안 된다.
		 *
		 * UE_DEFINE_GAMEPLAY_TAG 가 만드는 네이티브 태그는 태그 매니저가 초기화될 때
		 * 실제 값이 채워진다. 전역 배열은 그보다 먼저 생성될 수 있고, 그러면 무효 태그를
		 * 복사한 채로 굳어 버린다 — 전이가 통째로 죽는데 크래시는 나지 않아 원인을 찾기 어렵다.
		 * 지역 정적은 첫 호출 시점에 초기화되므로 그 순서 문제가 없다.
		 */
		static const TArray<FGameplayTag> Order = {
			HHTags::Phase_Prep,
			HHTags::Phase_Heist,
			HHTags::Phase_Escape,
			HHTags::Phase_Result
		};

		return Order;
	}

	int32 IndexOf(const FGameplayTag& Phase)
	{
		return GetOrder().IndexOfByKey(Phase);
	}

	FGameplayTag GetNext(const FGameplayTag& Phase)
	{
		const TArray<FGameplayTag>& Order = GetOrder();
		const int32 Index = Order.IndexOfByKey(Phase);

		if (Index == INDEX_NONE || !Order.IsValidIndex(Index + 1))
		{
			return FGameplayTag();   // 마지막이거나 우리 페이즈가 아니다
		}

		return Order[Index + 1];
	}

	const TCHAR* ToString(EHeistPhaseReason Reason)
	{
		switch (Reason)
		{
		case EHeistPhaseReason::Scheduled:  return TEXT("시간 만료");
		case EHeistPhaseReason::Alarm:      return TEXT("경보");
		case EHeistPhaseReason::AllEscaped: return TEXT("전원 탈출");
		case EHeistPhaseReason::Cheat:      return TEXT("치트");
		}

		return TEXT("알 수 없음");
	}
}
