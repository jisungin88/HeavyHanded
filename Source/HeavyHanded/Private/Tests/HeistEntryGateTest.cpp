#include "Misc/AutomationTest.h"

#include "Core/HeistEntryGate.h"

#if WITH_DEV_AUTOMATION_TESTS

// ──────────────────────────────────────────────────────────────
// 진입점 판정 (HeistEntryGate::Resolve)
//
// 이 판정이 틀려도 크래시가 나지 않는다. "고른 데서 안 나오네" 로만 드러나고,
// 레벨 디자이너가 진입점을 옮기거나 지울 때마다 그 경로를 밟게 된다.
// 폴백이 결정적(deterministic)인지가 특히 중요하다 — 무작위로 떨어지면
// "가끔 다른 데서 시작" 이 되어 재현 자체가 안 된다.
// ──────────────────────────────────────────────────────────────

namespace
{
	/** 저택의 진입점 3개. 실제 레벨과 같이 태그 이름순으로 정렬돼 있다 (CollectEntryPoints 의 계약) */
	TArray<FGameplayTag> MakeMansionEntries()
	{
		return {
			FGameplayTag::RequestGameplayTag(TEXT("Entry.Mansion.Alley"),  /*ErrorIfNotFound=*/false),
			FGameplayTag::RequestGameplayTag(TEXT("Entry.Mansion.Front"),  /*ErrorIfNotFound=*/false),
			FGameplayTag::RequestGameplayTag(TEXT("Entry.Mansion.Garage"), /*ErrorIfNotFound=*/false)
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistEntryGateTest,
	"HeavyHanded.Heist.EntryGate.Decisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistEntryGateTest::RunTest(const FString& Parameters)
{
	const TArray<FGameplayTag> Entries = MakeMansionEntries();

	// 태그가 .ini 에 없으면 이 테스트가 검증하는 것이 사라진다.
	// 무효 태그끼리는 전부 같아 보여서 "고른 것을 찾았다" 가 우연히 통과한다
	for (const FGameplayTag& Tag : Entries)
	{
		if (!TestTrue(TEXT("Entry.Mansion.* 태그가 Config/Tags/Phase.ini 에 등록돼 있어야 한다"), Tag.IsValid()))
		{
			return false;
		}
	}

	// ── 고른 진입점이 레벨에 있으면 그대로 쓴다 ──
	{
		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Entries, Entries[2]);

		TestEqual(TEXT("고른 진입점이 있으면 Selected"),
			Result.Decision, EHeistEntryDecision::Selected);
		TestEqual(TEXT("고른 그 진입점을 가리켜야 한다"), Result.Index, 2);
	}

	// ── 아무것도 안 골랐으면 첫 번째로 떨어진다 ──
	//
	// 은신처를 거치지 않고 레벨을 바로 연 경우(PIE)가 이 경로다. 실패가 아니라 정상 동작이다
	{
		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Entries, FGameplayTag());

		TestEqual(TEXT("선택이 없으면 Fallback"),
			Result.Decision, EHeistEntryDecision::Fallback);
		TestEqual(TEXT("폴백은 첫 번째다"), Result.Index, 0);
	}

	// ── 고른 진입점이 이 레벨에 없으면 첫 번째로 떨어진다 ──
	//
	// 레벨에서 진입점을 지웠거나, 저택에서 고른 것을 들고 박물관에 온 경우다.
	// 여기서 조용히 실패하면 "왜 자꾸 정문에서 시작하지" 가 된다 — 호출부가 경고를 남긴다
	{
		const FGameplayTag Foreign = FGameplayTag::RequestGameplayTag(
			TEXT("Entry.Mansion.Front"), /*ErrorIfNotFound=*/false);

		TArray<FGameplayTag> Others = { Entries[0], Entries[2] };   // Front 를 뺀 레벨

		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Others, Foreign);

		TestEqual(TEXT("이 레벨에 없는 태그면 Fallback"),
			Result.Decision, EHeistEntryDecision::Fallback);
		TestEqual(TEXT("폴백은 첫 번째다"), Result.Index, 0);
	}

	// ── 지정된 기본 진입점이 있으면 거기로 떨어진다 ──
	//
	// 이것이 없으면 기본 시작 위치가 태그 이름 알파벳순으로 정해진다.
	// Entry.Mansion.Alley 를 추가하는 것만으로 기본값이 정문에서 뒷골목으로 옮겨간다
	{
		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Entries, FGameplayTag(), /*DefaultIndex=*/1);

		TestEqual(TEXT("선택이 없으면 Fallback"),
			Result.Decision, EHeistEntryDecision::Fallback);
		TestEqual(TEXT("지정된 기본 진입점으로 떨어져야 한다"), Result.Index, 1);
	}

	// 고른 것이 이 레벨에 없을 때도 기본 진입점으로 간다 — 첫 번째가 아니다
	{
		const FGameplayTag Foreign = FGameplayTag::RequestGameplayTag(
			TEXT("Entry.Mansion.Front"), /*ErrorIfNotFound=*/false);

		TArray<FGameplayTag> Others = { Entries[0], Entries[2] };

		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Others, Foreign, /*DefaultIndex=*/1);

		TestEqual(TEXT("없는 태그여도 기본 진입점으로 떨어진다"), Result.Index, 1);
	}

	// 기본 지정이 범위를 벗어나면 첫 번째로 떨어진다.
	// 호출부가 인덱스를 잘못 계산했을 때 배열 밖을 짚는 것보다 낫다
	{
		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Entries, FGameplayTag(), /*DefaultIndex=*/99);

		TestEqual(TEXT("범위 밖 기본 지정은 첫 번째로 떨어진다"), Result.Index, 0);
	}

	// 기본 지정이 있어도 **고른 것이 우선**이다. 순서가 뒤집히면 선택이 무시된다
	{
		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Entries, Entries[2], /*DefaultIndex=*/0);

		TestEqual(TEXT("고른 것이 기본값보다 우선"),
			Result.Decision, EHeistEntryDecision::Selected);
		TestEqual(TEXT("고른 그 진입점이어야 한다"), Result.Index, 2);
	}

	// ── 진입점이 하나도 없으면 None ──
	//
	// 테스트 맵이 이 경우다. 호출부는 엔진 기본 스폰으로 돌아가야 하고,
	// Index 로 배열을 건드리면 안 된다
	{
		const FHeistEntryResolution Result = HeistEntryGate::Resolve({}, Entries[0]);

		TestEqual(TEXT("진입점이 없으면 None"),
			Result.Decision, EHeistEntryDecision::None);
		TestEqual(TEXT("None 이면 인덱스가 없어야 한다"), Result.Index, (int32)INDEX_NONE);
	}

	// ── 부모 태그로는 걸리지 않는다 ──
	//
	// Entry.Mansion 하나가 Entry.Mansion.Front 를 가리키게 되면, 진입점이 늘어날 때
	// 예전 선택이 조용히 다른 곳으로 옮겨간다. 정확히 일치하는 것만 본다
	{
		const FGameplayTag Parent = FGameplayTag::RequestGameplayTag(
			TEXT("Entry.Mansion"), /*ErrorIfNotFound=*/false);

		const FHeistEntryResolution Result = HeistEntryGate::Resolve(Entries, Parent);

		TestEqual(TEXT("부모 태그는 선택으로 인정되지 않는다"),
			Result.Decision, EHeistEntryDecision::Fallback);
	}

	// ── 같은 입력이면 항상 같은 답 ──
	//
	// 폴백이 결정적이지 않으면 "가끔 다른 데서 시작" 이 되어 재현이 안 된다
	{
		const FHeistEntryResolution First  = HeistEntryGate::Resolve(Entries, FGameplayTag());
		const FHeistEntryResolution Second = HeistEntryGate::Resolve(Entries, FGameplayTag());

		TestEqual(TEXT("폴백은 결정적이어야 한다"), First.Index, Second.Index);
	}

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
