#include "Misc/AutomationTest.h"

#include "Core/HeistSettings.h"
#include "Core/HeistTravel.h"

#if WITH_DEV_AUTOMATION_TESTS

// ──────────────────────────────────────────────────────────────
// 출발 URL (HeistTravel::BuildTravelURL)
//
// 이게 틀려도 맵은 정상적으로 열린다. 전원이 저택에 도착하고, 화면상 아무 문제가 없다.
// 다만 ?ExpectedPlayers 가 빠지면 AHeistGameMode 가 인원을 모르는 채로 시작해서
// 조용 시간 폴백으로 떨어지고, 그 경로의 증상은 "가끔 한 명 두고 출발하더라" 다.
//
// 즉 실패가 며칠 뒤에 간헐적으로 드러난다. 옵션 이름은
// AHeistGameMode::ResolveExpectedPlayers 의 GetIntOption 문자열과 짝인데 서로를 모르므로,
// 그 짝을 여기서 못박는다.
// ──────────────────────────────────────────────────────────────

namespace
{
	/** 실제로 커밋된 저택 레벨. 에셋을 로드하지 않는다 — 경로 문자열만 쓴다 */
	const TCHAR* MansionPath = TEXT("/Game/HeavyHanded/Maps/Mansion/L_Mansion.L_Mansion");

	/** AHeistGameMode 가 읽는 옵션 이름. 이 문자열이 계약이다 */
	const TCHAR* ExpectedPlayersOption = TEXT("ExpectedPlayers");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistTravelURLTest,
	"HeavyHanded.Heist.Travel.URLCarriesExpectedPlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistTravelURLTest::RunTest(const FString& Parameters)
{
	// ── 에셋 이름이 아니라 패키지 이름으로 떠난다 ──
	//
	// 소프트 경로는 "/Game/.../L_Mansion.L_Mansion" 이다. 뒤쪽 에셋 이름을 떼지 않고
	// 그대로 넘기면 ServerTravel 이 맵을 찾지 못한다
	{
		const FString URL = HeistTravel::BuildTravelURL(FSoftObjectPath(MansionPath), 2);

		TestTrue(TEXT("패키지 이름으로 시작해야 한다"),
			URL.StartsWith(TEXT("/Game/HeavyHanded/Maps/Mansion/L_Mansion")));
		TestFalse(TEXT("에셋 이름(.L_Mansion)이 남아 있으면 안 된다"),
			URL.Contains(TEXT("L_Mansion.L_Mansion")));
	}

	// ── 대기 인원이 옵션으로 실려 간다 ──
	//
	// 이름과 값 둘 다 본다. 이름만 맞고 값이 안 붙으면 GetIntOption 이 0 을 돌려줘서
	// 옵션을 안 넘긴 것과 같아진다
	{
		const FString URL = HeistTravel::BuildTravelURL(FSoftObjectPath(MansionPath), 4);

		TestTrue(TEXT("옵션 이름이 AHeistGameMode 가 읽는 것과 같아야 한다"),
			URL.Contains(FString::Printf(TEXT("?%s="), ExpectedPlayersOption)));
		TestTrue(TEXT("인원 수가 값으로 실려야 한다"),
			URL.EndsWith(FString::Printf(TEXT("?%s=4"), ExpectedPlayersOption)));
	}

	// ── 인원을 모르면 옵션을 붙이지 않는다 ──
	//
	// PIE 로 레벨을 직접 여는 경로다. 0 을 그대로 실어 보내면 저택 쪽이 "0명을 기다린다" 로
	// 읽을 여지가 생긴다 — 아예 안 보내야 폴백 판정으로 정확히 떨어진다
	{
		const FString URL = HeistTravel::BuildTravelURL(FSoftObjectPath(MansionPath), 0);

		TestFalse(TEXT("인원이 0이면 옵션이 없어야 한다"),
			URL.Contains(ExpectedPlayersOption));
		TestFalse(TEXT("그래도 맵 경로는 있어야 한다"), URL.IsEmpty());
	}

	// 음수는 0 과 같게 다룬다. 명단이 비었을 때 호출부가 -1 을 넘길 여지가 있다
	{
		const FString URL = HeistTravel::BuildTravelURL(FSoftObjectPath(MansionPath), -1);

		TestFalse(TEXT("음수 인원도 옵션을 붙이지 않는다"), URL.Contains(ExpectedPlayersOption));
	}

	// ── 레벨이 없으면 빈 문자열이다 ──
	//
	// 여기가 이 함수에서 가장 중요한 줄이다. 빈 경로에 폴백이 있으면 장소 매핑을 빠뜨린 것을
	// **엉뚱한 레벨이 열리는 것**으로 알게 되고, 그때는 전원이 이미 그리로 끌려간 뒤다
	{
		const FString URL = HeistTravel::BuildTravelURL(FSoftObjectPath(), 4);

		TestTrue(TEXT("레벨 경로가 비면 URL 도 비어야 한다 — 떠나면 안 된다"), URL.IsEmpty());
	}

	return true;
}

// ──────────────────────────────────────────────────────────────
// 장소 매핑 (UHeistSettings::SiteLevels)
//
// 이 표는 Config/DefaultHeistSystem.ini 에 한 줄로 들어 있는데, 그 줄의 문법이 틀리면
// **경고 없이 배열이 비어 버린다.** 태그 파일의 `+` 함정(문서 04 3장)과 같은 종류다 —
// 다른 점은 이쪽이 config 계층이라 `+` 가 맞다는 것뿐이고, 침묵하는 것은 똑같다.
//
// 비어 있으면 은신처에서 출발이 그냥 안 된다. 그 사실을 오후 통합 테스트에서
// "출발 버튼이 안 먹네" 로 알게 되면 원인까지 가는 데 한참 걸린다.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSiteLevelsTest,
	"HeavyHanded.Heist.Travel.SiteLevelsResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSiteLevelsTest::RunTest(const FString& Parameters)
{
	const UHeistSettings* Settings = UHeistSettings::Get();

	if (!TestFalse(TEXT("SiteLevels 가 비어 있다 — Config/DefaultHeistSystem.ini 의 "
					   "+SiteLevels 줄이 파싱되지 않았을 수 있다"),
			Settings->SiteLevels.IsEmpty()))
	{
		return false;
	}

	// 절반만 파싱된 행을 잡는다. 태그나 경로 한쪽만 비어도 그 장소는 못 가는데,
	// 배열이 비지는 않아서 위 검사는 통과한다
	for (const FHeistSiteLevel& Entry : Settings->SiteLevels)
	{
		TestTrue(TEXT("SiteLevels 의 태그가 Config/Tags/Phase.ini 에 등록돼 있어야 한다"),
			Entry.SiteTag.IsValid());
		TestFalse(FString::Printf(TEXT("%s 의 레벨 경로가 비어 있다"), *Entry.SiteTag.ToString()),
			Entry.Level.IsNull());
	}

	// ── 저택은 반드시 갈 수 있어야 한다 ──
	//
	// 기획서 2장의 첫 장소다. 여기가 막히면 코어 루프 전체를 플레이해 볼 수 없다
	{
		const FGameplayTag Mansion =
			FGameplayTag::RequestGameplayTag(TEXT("Site.Mansion"), /*ErrorIfNotFound=*/false);

		if (TestTrue(TEXT("Site.Mansion 태그가 등록돼 있어야 한다"), Mansion.IsValid()))
		{
			const FSoftObjectPath LevelPath = Settings->GetSiteLevel(Mansion);

			TestFalse(TEXT("Site.Mansion 의 레벨이 SiteLevels 에 등록돼 있어야 한다"),
				LevelPath.IsNull());
			TestFalse(TEXT("그 경로로 출발 URL 을 만들 수 있어야 한다"),
				HeistTravel::BuildTravelURL(LevelPath, 2).IsEmpty());
		}
	}

	// 등록되지 않은 장소는 빈 경로다. 이게 깨지면 오타 난 태그가 엉뚱한 맵을 연다
	{
		const FGameplayTag Unknown =
			FGameplayTag::RequestGameplayTag(TEXT("Site.Museum"), /*ErrorIfNotFound=*/false);

		if (Unknown.IsValid() && !Settings->GetSiteLevel(Unknown).IsNull())
		{
			// 박물관 맵이 생겨서 등록된 것이라면 정상이다. 그때는 이 블록을 지운다
			AddInfo(TEXT("Site.Museum 이 등록돼 있다 — 박물관 맵이 생겼다면 정상이다"));
		}
	}

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
