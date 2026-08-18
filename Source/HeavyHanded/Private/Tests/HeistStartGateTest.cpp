#include "Misc/AutomationTest.h"

#include "Core/HeistPhase.h"
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

#endif   // WITH_DEV_AUTOMATION_TESTS
