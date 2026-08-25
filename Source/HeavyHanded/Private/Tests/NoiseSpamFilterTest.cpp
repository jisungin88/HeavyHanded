#include "Misc/AutomationTest.h"

#include "Core/HeavyHandedGameplayTags.h"
#include "Noise/NoiseSpamFilter.h"

#if WITH_DEV_AUTOMATION_TESTS

// ──────────────────────────────────────────────────────────────
// FNoiseSpamFilter 회귀 테스트
//
// 밸런싱 값이 계속 바뀔 자리인데 잘못 고쳐도 "경비가 소리를 못 듣는다" 로만 드러난다.
// 그래서 (입력 시퀀스 → 발행 시퀀스) 를 여기서 못박는다.
// 실행: Window > Test Automation > HeavyHanded.Noise.SpamFilter
// ──────────────────────────────────────────────────────────────

namespace
{
	constexpr float TestCooldown = 0.25f;

	/** 테스트 태그. 프로덕션과 같은 상수를 쓴다 — 문자열을 복사하면 테스트가 자기 복사본을 검증하게 된다 */
	FGameplayTag ImpactTag()
	{
		return HHTags::Noise_Loot_Impact;
	}

	/** 파손형 노획물이 깨질 때 나가는 태그. 충돌 태그와 상태가 분리돼 있다 */
	FGameplayTag BreakTag()
	{
		return HHTags::Noise_Loot_Break;
	}

	FNoiseSpamFilterParams DefaultParams()
	{
		FNoiseSpamFilterParams Params;
		Params.ConsecutiveFalloff      = 0.7f;
		Params.ConsecutiveFloor        = 0.2f;
		Params.ConsecutiveResetSeconds = 2.f;
		return Params;
	}

	/**
	 * 컴포넌트가 하는 일을 그대로 흉내낸다: Advance 로 뽑고 -> 발행했다고 통보.
	 * @return 이번 스텝에서 나간 발행 건수
	 */
	int32 StepAndEmit(FNoiseSpamFilter& Filter, float DeltaTime, FNoisePendingEmitArray& OutAll)
	{
		FNoisePendingEmitArray Pending;
		Filter.Advance(DeltaTime, Pending);

		for (const FNoisePendingEmit& Emit : Pending)
		{
			Filter.NotifyEmitted(Emit.Tag, Emit.Loudness, TestCooldown);
			OutAll.Add(Emit);
		}

		return Pending.Num();
	}
}

// ──────────────────────────────────────────────────────────────
// 이 테스트가 잡으려는 회귀 —
// Advance 에 "직전 발행보다 커야 발행한다" 조건이 있으면 발행이 1건에서 끝난다.
// 경비 인지 게이지(GainPerStimulus 0.35)는 자극 3건이 있어야 100% 에 닿으므로,
// 그 조건이 살아 있으면 프롭을 굴려서 경비를 부르는 것이 원리적으로 불가능해진다.
// ──────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseSpamFilterRollingPropTest,
	"HeavyHanded.Noise.SpamFilter.RollingPropStaysAudible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNoiseSpamFilterRollingPropTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Tag = ImpactTag();
	if (!TestTrue(TEXT("Noise.Loot.Impact 태그가 등록돼 있어야 한다"), Tag.IsValid()))
	{
		return false;
	}

	FNoiseSpamFilter Filter;
	Filter.Params = DefaultParams();

	FNoisePendingEmitArray Emitted;

	// 2초간 60fps 로 굴린다. 매 프레임 같은 크기로 부딪히는 최악의 스팸
	constexpr float Dt = 1.f / 60.f;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Filter.Submit(Tag, 0.8f, FVector::ZeroVector);
		StepAndEmit(Filter, Dt, Emitted);
	}

	// 쿨다운 0.25s 로 2초면 8건 언저리. 정확한 수는 프레임 경계에 걸려 ±1 흔들린다
	TestTrue(FString::Printf(TEXT("발행이 여러 건 나와야 한다 (실제 %d건)"), Emitted.Num()),
			 Emitted.Num() >= 6);
	TestTrue(FString::Printf(TEXT("120번 충돌이 그대로 새어나가면 안 된다 (실제 %d건)"), Emitted.Num()),
			 Emitted.Num() <= 12);

	// 경비에게는 매번 들려야 한다 — Loudness 는 절대값이고 0 이 아니다
	for (const FNoisePendingEmit& Emit : Emitted)
	{
		TestTrue(TEXT("모든 발행의 Loudness 는 0 보다 커야 한다"), Emit.Loudness > 0.f);
	}

	// 억제는 경계도 축에서만. 첫 건은 온전히, 그 뒤로는 새로운 정보가 없으므로 0
	TestEqual(TEXT("첫 발행의 AlertScale 은 1.0"), Emitted[0].AlertScale, 1.f, KINDA_SMALL_NUMBER);

	float AlertSum = 0.f;
	for (const FNoisePendingEmit& Emit : Emitted)
	{
		AlertSum += Emit.AlertScale;
	}
	TestTrue(FString::Printf(TEXT("한 번 굴린 것으로 경계도가 차면 안 된다 (AlertScale 합 %.2f)"), AlertSum),
			 AlertSum < 2.f);

	return true;
}

// ──────────────────────────────────────────────────────────────
// 체감 지수는 "발행" 마다 오른다. 히트마다 오르면 한 프레임 안에 바닥을 쳐서
// ConsecutiveFalloff 가 튜닝 값 구실을 못한다.
// ──────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseSpamFilterFalloffPerEmitTest,
	"HeavyHanded.Noise.SpamFilter.FalloffCountsEmitsNotHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNoiseSpamFilterFalloffPerEmitTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Tag = ImpactTag();
	if (!TestTrue(TEXT("Noise.Loot.Impact 태그가 등록돼 있어야 한다"), Tag.IsValid()))
	{
		return false;
	}

	FNoiseSpamFilter Filter;
	Filter.Params = DefaultParams();

	FNoisePendingEmitArray Emitted;

	// 한 프레임에 50번 부딪히고 곧바로 발행. 아직 발행이 0건이므로 체감이 없어야 한다
	for (int32 i = 0; i < 50; ++i)
	{
		Filter.Submit(Tag, 1.f, FVector::ZeroVector);
	}
	StepAndEmit(Filter, 1.f / 60.f, Emitted);

	if (!TestEqual(TEXT("첫 틱에 1건만 나가야 한다"), Emitted.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("첫 발행은 체감을 받지 않는다 (히트마다 올렸다면 0.2 로 바닥)"),
			  Emitted[0].Loudness, 1.f, KINDA_SMALL_NUMBER);

	// 쿨다운을 넘긴 뒤 두 번째 발행 — 이제 발행 1회분 체감(0.7)이 걸린다
	Filter.Submit(Tag, 1.f, FVector::ZeroVector);
	Emitted.Reset();
	StepAndEmit(Filter, TestCooldown, Emitted);

	if (!TestEqual(TEXT("두 번째 발행이 나가야 한다"), Emitted.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("두 번째 발행은 0.7 배"), Emitted[0].Loudness, 0.7f, KINDA_SMALL_NUMBER);

	return true;
}

// ──────────────────────────────────────────────────────────────
// 파손형 노획물의 마지막 한 방. 소리를 낸 프레임에 그대로 파괴되면
// 대기 중인 소음이 사라지므로 FlushAll 이 건져야 한다.
// ──────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseSpamFilterFlushTest,
	"HeavyHanded.Noise.SpamFilter.FlushOnDestroyKeepsLastNoise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNoiseSpamFilterFlushTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Impact = ImpactTag();
	const FGameplayTag Break  = BreakTag();
	if (!TestTrue(TEXT("Noise.Loot.Impact / Noise.Loot.Break 태그가 등록돼 있어야 한다"),
				  Impact.IsValid() && Break.IsValid()))
	{
		return false;
	}

	FNoiseSpamFilter Filter;
	Filter.Params = DefaultParams();

	FNoisePendingEmitArray Emitted;

	// 1) 부딪히면서 충돌 소음이 한 번 나가고 쿨다운에 들어간다
	Filter.Submit(Impact, 1.f, FVector::ZeroVector);
	StepAndEmit(Filter, 1.f / 60.f, Emitted);
	TestEqual(TEXT("충돌 소음 첫 발행"), Emitted.Num(), 1);

	// 2) 같은 프레임에 깨지면서 파괴 소음(다른 태그)이 접수되고, 액터가 그대로 파괴된다.
	//    태그가 다르므로 상태도 별개다 — 충돌 쪽 연속 체감을 물려받지 않는다
	Filter.Submit(Break, 0.9f, FVector(1.f, 2.f, 3.f));

	FNoisePendingEmitArray Flushed;
	Filter.FlushAll(Flushed);

	if (!TestEqual(TEXT("파괴 소음 한 건이 건져져야 한다"), Flushed.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("발행 이력이 없는 태그라 체감 없이 절대값 그대로"),
			  Flushed[0].Loudness, 0.9f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("접수 지점이 보존돼야 한다"), Flushed[0].Location, FVector(1.f, 2.f, 3.f));
	TestEqual(TEXT("첫 발행이므로 경계도에 온전히 반영된다"),
			  Flushed[0].AlertScale, 1.f, KINDA_SMALL_NUMBER);

	// 3) 같은 태그로 이어서 접수하면 연속 체감이 걸린다는 것도 함께 못박는다.
	//    Flush 는 쿨다운만 무시할 뿐 체감까지 무시하지는 않는다
	Filter.NotifyEmitted(Break, Flushed[0].Loudness, TestCooldown);
	Filter.Submit(Break, 0.9f, FVector::ZeroVector);

	FNoisePendingEmitArray SecondFlush;
	Filter.FlushAll(SecondFlush);

	if (!TestEqual(TEXT("두 번째 파괴 소음도 건져진다"), SecondFlush.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("발행 1회분 체감(0.7)이 걸린다"),
			  SecondFlush[0].Loudness, 0.9f * 0.7f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("직전 발행보다 작으므로 경계도 기여는 0"),
			  SecondFlush[0].AlertScale, 0.f, KINDA_SMALL_NUMBER);

	return true;
}

// ──────────────────────────────────────────────────────────────
// 조용해지면 상태가 스스로 사라져야 한다.
// Advance 가 false 를 돌려주는 것이 컴포넌트가 틱을 끄는 신호다.
// ──────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseSpamFilterIdleTest,
	"HeavyHanded.Noise.SpamFilter.GoesIdleWhenQuiet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNoiseSpamFilterIdleTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Tag = ImpactTag();
	if (!TestTrue(TEXT("Noise.Loot.Impact 태그가 등록돼 있어야 한다"), Tag.IsValid()))
	{
		return false;
	}

	FNoiseSpamFilter Filter;
	Filter.Params = DefaultParams();

	FNoisePendingEmitArray Emitted;
	Filter.Submit(Tag, 0.5f, FVector::ZeroVector);
	StepAndEmit(Filter, 1.f / 60.f, Emitted);

	TestFalse(TEXT("발행 직후에는 상태가 살아 있어야 한다 (쿨다운이 남아 있다)"), Filter.IsEmpty());

	// 접수 없이 시간만 흘린다. 쿨다운 0.25 + 연속 리셋 2.0 을 넉넉히 넘긴다
	bool bAnyActive = true;
	for (int32 Frame = 0; Frame < 200 && bAnyActive; ++Frame)
	{
		FNoisePendingEmitArray Pending;
		bAnyActive = Filter.Advance(1.f / 60.f, Pending);
		TestEqual(TEXT("접수가 없으면 발행도 없어야 한다"), Pending.Num(), 0);
	}

	TestFalse(TEXT("결국 Advance 가 false 를 돌려줘야 틱이 꺼진다"), bAnyActive);
	TestTrue(TEXT("상태 맵이 비어야 한다 — 안 그러면 프롭마다 영구히 쌓인다"), Filter.IsEmpty());

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
