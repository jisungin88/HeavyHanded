#include "Misc/AutomationTest.h"

#include "Core/HeistPhase.h"
#include "Core/HeistEscapeGate.h"
#include "Core/HeistOutcome.h"
#include "Core/HeistStartGate.h"
#include "Core/HeavyHandedGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

// ──────────────────────────────────────────────────────────────
// 접속 대기 판정 (HeistStartGate::Evaluate)
//
// 이 판정이 틀리면 크래시가 나지 않는다. "가끔 한 명 두고 출발하더라" 로만 드러나고,
// 재현하려면 팀원 넷을 모아 로딩 속도를 각기 다르게 만들어야 한다 — 사람이 잡을 수 없다.
// 판정을 월드 모르는 순수 함수로 뽑아 둔 이유가 이것이다.
// ──────────────────────────────────────────────────────────────

namespace
{
	/** 아무 조건도 걸리지 않은 기본 상황. 각 테스트는 여기서 한두 값만 비튼다 */
	FHeistStartConditions MakeBaseline()
	{
		FHeistStartConditions Conditions;
		Conditions.NumPlayers = 1;
		Conditions.NumTravellingPlayers = 0;
		Conditions.ExpectedPlayers = 0;
		Conditions.SecondsSinceLastLogin = 0.f;
		Conditions.SecondsUntilDeadline = 10.f;
		Conditions.QuietSeconds = 3.f;

		return Conditions;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistStartGateTest,
	"HeavyHanded.Heist.StartGate.Decisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistStartGateTest::RunTest(const FString& Parameters)
{
	using namespace HeistStartGate;

	// ── 상한 ──
	{
		// 상한은 다른 어떤 조건보다 먼저 걸려야 한다. 안전망이 규칙에 막히면 안전망이 아니다
		FHeistStartConditions C = MakeBaseline();
		C.SecondsUntilDeadline = 0.f;
		C.ExpectedPlayers = 4;
		C.NumPlayers = 1;
		C.NumTravellingPlayers = 2;
		TestEqual(TEXT("상한을 넘기면 로딩 중이 있어도 시작한다"),
			Evaluate(C), EHeistStartDecision::TimedOut);
	}

	// ── 로딩 중 인원 ──
	{
		FHeistStartConditions C = MakeBaseline();
		C.ExpectedPlayers = 2;
		C.NumPlayers = 2;              // 인원은 다 찼지만
		C.NumTravellingPlayers = 1;    // 한 명이 아직 로딩 화면이다
		TestEqual(TEXT("인원이 다 차도 로딩 중이 있으면 기다린다"),
			Evaluate(C), EHeistStartDecision::Wait);
	}
	{
		// 올 인원을 몰라도 로딩 중은 막을 수 있다 — 폴백 경로의 유일한 보호막이다
		FHeistStartConditions C = MakeBaseline();
		C.ExpectedPlayers = 0;
		C.NumTravellingPlayers = 1;
		C.SecondsSinceLastLogin = 99.f;   // 한참 조용했더라도
		TestEqual(TEXT("인원을 몰라도 로딩 중이면 기다린다"),
			Evaluate(C), EHeistStartDecision::Wait);
	}

	// ── 예정 인원을 아는 경우 (정상 경로) ──
	{
		// 팀장 지적 사항의 회귀 테스트.
		// 호스트 + 1명이 들어오고 한참 조용해도, 4명이 올 예정이면 두고 출발하지 않는다
		FHeistStartConditions C = MakeBaseline();
		C.ExpectedPlayers = 4;
		C.NumPlayers = 2;
		C.SecondsSinceLastLogin = 99.f;
		TestEqual(TEXT("예정 인원이 안 차면 아무리 조용해도 기다린다"),
			Evaluate(C), EHeistStartDecision::Wait);
	}
	{
		FHeistStartConditions C = MakeBaseline();
		C.ExpectedPlayers = 4;
		C.NumPlayers = 4;
		C.SecondsSinceLastLogin = 0.f;   // 방금 들어왔어도
		TestEqual(TEXT("예정 인원이 다 차고 로딩도 끝났으면 바로 시작한다"),
			Evaluate(C), EHeistStartDecision::Ready);
	}
	{
		// 재접속 등으로 예정보다 많아지는 경우. 초과는 대기 사유가 아니다
		FHeistStartConditions C = MakeBaseline();
		C.ExpectedPlayers = 2;
		C.NumPlayers = 3;
		TestEqual(TEXT("예정 인원을 넘겨도 시작한다"),
			Evaluate(C), EHeistStartDecision::Ready);
	}

	// ── 예정 인원을 모르는 경우 (폴백) ──
	{
		FHeistStartConditions C = MakeBaseline();
		C.SecondsSinceLastLogin = 1.f;   // QuietSeconds 3초에 못 미친다
		TestEqual(TEXT("인원을 모르고 아직 조용하지 않으면 기다린다"),
			Evaluate(C), EHeistStartDecision::Wait);
	}
	{
		FHeistStartConditions C = MakeBaseline();
		C.SecondsSinceLastLogin = 3.f;   // 경계값 — 딱 맞으면 시작한다
		TestEqual(TEXT("인원을 모르고 조용해지면 시작한다"),
			Evaluate(C), EHeistStartDecision::Ready);
	}

	return true;
}

// ──────────────────────────────────────────────────────────────
// 페이즈 전이표 (HeistPhase)
//
// 전이가 배열 하나에서 나오므로, 그 배열이 온전한지만 지키면 전이 전체가 지켜진다.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistPhaseOrderTest,
	"HeavyHanded.Heist.Phase.Order",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistPhaseOrderTest::RunTest(const FString& Parameters)
{
	const TArray<FGameplayTag>& Order = HeistPhase::GetOrder();

	// 네이티브 태그가 등록되기 전에 배열이 굳으면 전부 무효 태그가 된다.
	// 크래시가 안 나고 전이만 조용히 죽는 종류의 사고라 여기서 잡는다
	for (const FGameplayTag& Phase : Order)
	{
		TestTrue(TEXT("순서 배열에 무효 태그가 없어야 한다"), Phase.IsValid());
	}

	TestEqual(TEXT("순서는 Prep → Heist → Escape → Result 네 단계다"), Order.Num(), 4);

	// 기획서 2장의 코어 루프 순서 그대로여야 한다.
	// HHTags::* 는 FNativeGameplayTag 라 GetTag() 로 풀어 준다 —
	// 그냥 넘기면 TestEqual 의 템플릿 추론이 두 타입 사이에서 갈린다
	TestEqual(TEXT("Prep 다음은 Heist"),
		HeistPhase::GetNext(HHTags::Phase_Prep), HHTags::Phase_Heist.GetTag());
	TestEqual(TEXT("Heist 다음은 Escape"),
		HeistPhase::GetNext(HHTags::Phase_Heist), HHTags::Phase_Escape.GetTag());
	TestEqual(TEXT("Escape 다음은 Result"),
		HeistPhase::GetNext(HHTags::Phase_Escape), HHTags::Phase_Result.GetTag());

	// 끝에서 한 칸 더 가려는 시도는 무효 태그로 막힌다.
	// 이게 뚫리면 Result 에서 타이머가 다시 돌기 시작한다
	TestFalse(TEXT("Result 다음은 없다"),
		HeistPhase::GetNext(HHTags::Phase_Result).IsValid());

	// 우리 페이즈가 아닌 태그를 넣어도 조용히 무효를 돌려줘야 한다
	TestFalse(TEXT("페이즈가 아닌 태그는 다음이 없다"),
		HeistPhase::GetNext(HHTags::Loot_Type_Heavy).IsValid());
	TestFalse(TEXT("빈 태그는 다음이 없다"),
		HeistPhase::GetNext(FGameplayTag()).IsValid());

	return true;
}

// ──────────────────────────────────────────────────────────────
// 탈출 판정 (HeistEscapeGate::HasEveryoneEscaped)
//
// 이것도 접속 대기와 같은 종류다 — 틀려도 크래시가 안 나고 "가끔 판이 일찍 끝나더라" 로만
// 드러난다. 재현하려면 넷이 모여 다운과 승차를 특정 순서로 만들어야 해서 눈으로 못 잡는다.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistEscapeGateTest,
	"HeavyHanded.Heist.EscapeGate.Decisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistEscapeGateTest::RunTest(const FString& Parameters)
{
	using namespace HeistEscapeGate;

	// ── 전원 다운 — 이 테스트가 존재하는 이유 ──
	{
		// 0명 중 0명이 탔으므로 산술적으로는 "전원 탈출" 이 참이다.
		// 그대로 두면 넷이 바닥에 쓰러져 있는데 탈출 성공으로 정산된다
		FHeistEscapeConditions C;
		C.NumActivePlayers = 4;
		C.NumDownedPlayers = 4;
		C.NumBoardedSurvivors = 0;
		TestFalse(TEXT("전원 다운은 전원 탈출이 아니다"), HasEveryoneEscaped(C));
	}

	// ── 정상 경로 ──
	{
		FHeistEscapeConditions C;
		C.NumActivePlayers = 4;
		C.NumDownedPlayers = 0;
		C.NumBoardedSurvivors = 4;
		TestTrue(TEXT("넷이 다 타면 끝난다"), HasEveryoneEscaped(C));

		C.NumBoardedSurvivors = 3;
		TestFalse(TEXT("한 명이 남아 있으면 안 끝난다"), HasEveryoneEscaped(C));
	}

	// ── 다운자는 분모에서 빠진다 ──
	{
		// 기획 확정 사항이다. 다운된 팀원을 두고 나머지가 타면 그 순간 판이 끝난다
		FHeistEscapeConditions C;
		C.NumActivePlayers = 4;
		C.NumDownedPlayers = 1;
		C.NumBoardedSurvivors = 3;
		TestTrue(TEXT("다운자를 뺀 생존자가 다 타면 끝난다"), HasEveryoneEscaped(C));

		TestEqual(TEXT("생존자는 접속자에서 다운자를 뺀 값이다"), GetSurvivorNum(C), 3);
	}

	// ── 혼자 하는 판 ──
	{
		FHeistEscapeConditions C;
		C.NumActivePlayers = 1;
		C.NumBoardedSurvivors = 1;
		TestTrue(TEXT("혼자여도 타면 끝난다"), HasEveryoneEscaped(C));
	}

	// ── 아무도 없다 ──
	{
		// 전원이 접속을 끊은 판이다. 여기서 참이 나오면 빈 서버가 결과 화면으로 넘어간다
		FHeistEscapeConditions C;
		TestFalse(TEXT("접속자가 없으면 끝나지 않는다"), HasEveryoneEscaped(C));
	}

	// ── 값이 어긋난 경우 ──
	{
		// 두 값이 서로 다른 시점에 세어지면 한 프레임 어긋날 수 있다.
		// 그때 판정이 뒤집히거나 영영 성립하지 않는 쪽으로 기울면 안 된다
		FHeistEscapeConditions C;
		C.NumActivePlayers = 2;
		C.NumDownedPlayers = 5;
		TestEqual(TEXT("다운자가 접속자보다 많아도 생존자는 음수가 되지 않는다"),
			GetSurvivorNum(C), 0);
		TestFalse(TEXT("그 상태에서 탈출로 판정하지 않는다"), HasEveryoneEscaped(C));

		C.NumActivePlayers = 3;
		C.NumDownedPlayers = 0;
		C.NumBoardedSurvivors = 4;   // 셋인데 넷이 탔다
		TestTrue(TEXT("승차 인원이 더 많아도 탈출로 본다"), HasEveryoneEscaped(C));
	}

	return true;
}

// ──────────────────────────────────────────────────────────────
// 결과 등급 (HeistOutcome::Evaluate)
//
// 규칙 자체는 두 줄이라 눈으로도 읽힌다. 그런데도 고정해 두는 이유는 이 값이
// 팀 골드 지급 여부를 가르기 때문이다 — 한 칸 어긋나면 한 판을 통째로 날린다.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistOutcomeTest,
	"HeavyHanded.Heist.Outcome.Grades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistOutcomeTest::RunTest(const FString& Parameters)
{
	using namespace HeistOutcome;

	TestEqual(TEXT("목표 달성 + 전원 탈출은 성공"),
		Evaluate(/*bTargetReached=*/true, /*bEveryoneEscaped=*/true), EHeistOutcome::Success);

	TestEqual(TEXT("돈은 챙겼지만 사람을 두고 왔으면 부분 성공"),
		Evaluate(true, false), EHeistOutcome::Partial);

	TestEqual(TEXT("무사히 나왔지만 목표를 못 채웠으면 부분 성공"),
		Evaluate(false, true), EHeistOutcome::Partial);

	TestEqual(TEXT("둘 다 아니면 실패"),
		Evaluate(false, false), EHeistOutcome::Failure);

	// 지급 기준선(UHeistSettings::MinOutcomeForPayout)이 이 순서에 기대어
	// 비교 연산 하나로 판정한다. 순서가 뒤집히면 실패한 판에 돈이 들어간다
	TestTrue(TEXT("등급은 Failure < Partial < Success 순서다"),
		EHeistOutcome::Failure < EHeistOutcome::Partial
		&& EHeistOutcome::Partial < EHeistOutcome::Success);

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
