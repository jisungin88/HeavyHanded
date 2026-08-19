#pragma once

#include "CoreMinimal.h"

/** 접속 대기 판정 결과 */
enum class EHeistStartDecision : uint8
{
	/** 아직 시작하면 안 된다 */
	Wait,

	/** 조건을 다 만족했다 — 바로 시작한다 */
	Ready,

	/** 상한 시간을 넘겼다. 못 온 사람을 두고 시작한다 (경고 대상) */
	TimedOut
};

/**
 * 판정에 필요한 사실 전부.
 *
 * 여기 없는 것은 판정에 영향을 주지 않는다 — 이 구조체가 곧 "무엇을 보고 정하는가" 의 목록이다.
 * 시각을 절대값이 아니라 경과·잔여로 받는 이유는, 월드 시계를 몰라도 판정할 수 있게 하기 위해서다.
 */
struct FHeistStartConditions
{
	/** 접속 완료 + 로딩 중을 합친 인원 (AGameMode::GetNumPlayers) */
	int32 NumPlayers = 0;

	/** 접속은 했으나 아직 레벨 로딩이 끝나지 않은 인원 (AGameMode::NumTravellingPlayers) */
	int32 NumTravellingPlayers = 0;

	/** 이 판에 올 인원. 0 이면 모른다는 뜻이고, 그때만 조용함 판정으로 떨어진다 */
	int32 ExpectedPlayers = 0;

	/** 마지막 접속 이후 흐른 시간(초) */
	float SecondsSinceLastLogin = 0.f;

	/** 상한까지 남은 시간(초). 0 이하면 상한을 넘긴 것이다 */
	float SecondsUntilDeadline = 0.f;

	/** 인원을 모를 때, 이만큼 조용하면 시작한다 */
	float QuietSeconds = 0.f;
};

/**
 * "지금 준비 시간을 시작해도 되는가" 판정.
 *
 * [왜 GameMode 밖으로 뺐는가] 이 판정은 규칙이 네 갈래로 갈리는데, 잘못돼도 크래시가 나지 않고
 *   "가끔 한 명 두고 출발하더라" 로만 드러난다. 그런 규칙은 사람이 눈으로 재현해서 잡을 수 없다.
 *
 *   월드·액터·타이머를 모르는 순수 함수로 두면 상황을 값으로 만들어 바로 검증할 수 있다.
 *   부수효과(타이머 재예약 · 로그 · 페이즈 진입)는 호출부인 AHeistGameMode 가 갖는다.
 *   Private/Tests/HeistStartGateTest.cpp 가 이 함수를 직접 부른다.
 *
 *   경계도 회귀 테스트(AlertLevelTest.cpp)가 "판정이 자유 함수로 나오면 직접 호출로 바꿀 것" 이라고
 *   남겨 둔 그 형태다.
 */
namespace HeistStartGate
{
	HEAVYHANDED_API EHeistStartDecision Evaluate(const FHeistStartConditions& Conditions);
}
