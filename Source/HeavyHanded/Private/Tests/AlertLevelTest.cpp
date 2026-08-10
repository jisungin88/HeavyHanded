#include "Misc/AutomationTest.h"

#include "Alert/AlertSettings.h"
#include "Noise/NoiseTypes.h"
#include "Shared/Gauge01.h"

#if WITH_DEV_AUTOMATION_TESTS

// ──────────────────────────────────────────────────────────────
// 경계도 단계 판정 회귀 테스트
//
// UAlertComponent::EvaluateLevel 은 히스테리시스 · 래치 · 2단계 점프가 얽혀 있어서
// 임계값을 한 칸 만지면 조용히 깨지기 좋다. 판정 자체는 (게이지, 현재단계) -> 단계 인
// 순수 함수라 월드 없이 검증할 수 있다.
//
// 다만 EvaluateLevel 은 컴포넌트의 private 이라 여기서 직접 못 부른다.
// 지금은 규칙을 여기에 복제해 두고 "임계값 세팅이 규칙을 만족하는가" 를 지킨다 —
// EvaluateLevel 이 FAlertLevelRules 같은 자유 함수로 나오면 그때 직접 호출로 바꿀 것.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlertThresholdSanityTest,
	"HeavyHanded.Alert.Thresholds.HysteresisNotInverted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAlertThresholdSanityTest::RunTest(const FString& Parameters)
{
	const UAlertSettings* Settings = UAlertSettings::Get();
	if (!TestNotNull(TEXT("UAlertSettings CDO 는 항상 존재해야 한다"), Settings))
	{
		return false;
	}

	// Exit 가 Enter 보다 높으면 그 단계에 갇히거나 영영 못 들어간다.
	// PostEditChangeProperty 가 막고 있지만, ini 를 손으로 고치면 그 훅을 안 탄다
	TestTrue(TEXT("SuspiciousExit < SuspiciousEnter"),
			 Settings->SuspiciousExit < Settings->SuspiciousEnter);
	TestTrue(TEXT("AlertedExit < AlertedEnter"),
			 Settings->AlertedExit < Settings->AlertedEnter);
	TestTrue(TEXT("의심 구간이 경계 구간보다 아래에 있어야 한다"),
			 Settings->SuspiciousEnter < Settings->AlertedEnter);

	// 히스테리시스 간격이 임계값 비교 여유(1e-4)보다 충분히 커야 의미가 있다.
	// 간격이 그 수준까지 좁아지면 단계가 다시 깜빡이기 시작한다
	TestTrue(TEXT("의심 히스테리시스 간격이 충분해야 한다"),
			 (Settings->SuspiciousEnter - Settings->SuspiciousExit) > 0.01f);
	TestTrue(TEXT("경계 히스테리시스 간격이 충분해야 한다"),
			 (Settings->AlertedEnter - Settings->AlertedExit) > 0.01f);

	// 경보는 1.0 래치다. Enter 가 1.0 이상이면 경계 단계를 건너뛴다
	TestTrue(TEXT("AlertedEnter 는 1.0 미만이어야 한다"), Settings->AlertedEnter < 1.f);

	return true;
}

// ──────────────────────────────────────────────────────────────
// 게이지 양자화 왕복 (Shared/Gauge01.h).
//
// UAlertComponent 와 UPerceptionMeterComponent 가 이 함수들로 uint8 복제를 한다.
// 되돌린 값의 오차가 반 스텝을 넘으면 클라 HUD 가 서버와 눈에 띄게 어긋난다.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGaugeQuantizationTest,
	"HeavyHanded.Alert.Quantization.RoundTripWithinHalfStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGaugeQuantizationTest::RunTest(const FString& Parameters)
{
	// 프로덕션 코드를 그대로 부른다.
	// 예전에는 여기에 람다 복사본을 두고 "여기가 깨지면 저쪽도 깨진 것" 이라고 적어놨는데,
	// 복사본이라 반대 방향이 성립하지 않았다 — 컴포넌트 쪽 식을 잘못 고쳐도
	// 이 테스트는 자기 람다를 돌려서 그대로 통과한다. 회귀를 잡으라고 넣은 테스트가
	// 정확히 그 회귀를 못 잡는 상태였다
	using Gauge01::Quantize;
	using Gauge01::Dequantize;

	constexpr float HalfStep = Gauge01::MaxRoundTripError;

	// 경계값 먼저 — 0 과 1 은 정확히 왕복해야 한다.
	// 1.0 이 254 로 떨어지면 "게이지는 꽉 찼는데 HUD 바가 안 찬" 상태가 된다
	TestEqual(TEXT("0.0 왕복"), Dequantize(Quantize(0.f)), 0.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("1.0 왕복"), Dequantize(Quantize(1.f)), 1.f, KINDA_SMALL_NUMBER);

	// 클램프. 범위 밖 입력이 반대편으로 넘어가면(래핑) 경보가 오작동한다
	TestEqual(TEXT("음수는 0 으로 클램프"), Quantize(-5.f), static_cast<uint8>(0));
	TestEqual(TEXT("1 초과는 255 로 클램프"), Quantize(12.f), static_cast<uint8>(255));

	// 전 구간 스윕
	for (int32 Step = 0; Step <= 1000; ++Step)
	{
		const float Original = static_cast<float>(Step) / 1000.f;
		const float RoundTrip = Dequantize(Quantize(Original));

		if (!TestTrue(FString::Printf(TEXT("%.4f 왕복 오차 %.6f 는 반 스텝(%.6f) 이내여야 한다"),
									  Original, FMath::Abs(RoundTrip - Original), HalfStep),
					  FMath::Abs(RoundTrip - Original) <= HalfStep + KINDA_SMALL_NUMBER))
		{
			return false;   // 첫 실패에서 멈춘다. 1000줄을 쏟을 이유가 없다
		}
	}

	// 단조성 — 게이지가 오르는데 복제값이 내려가면 HUD 바가 거꾸로 움직인다
	uint8 Previous = 0;
	for (int32 Step = 0; Step <= 1000; ++Step)
	{
		const uint8 Current = Quantize(static_cast<float>(Step) / 1000.f);
		if (!TestTrue(TEXT("양자화는 단조 증가해야 한다"), Current >= Previous))
		{
			return false;
		}
		Previous = Current;
	}

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
