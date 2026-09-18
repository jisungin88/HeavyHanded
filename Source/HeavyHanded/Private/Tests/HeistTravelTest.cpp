#include "Misc/AutomationTest.h"

#include "Engine/DataTable.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistSettings.h"
#include "Core/HeistSiteTypes.h"
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
// 장소 카탈로그 (DT_SiteCatalog)
//
// 이 표가 비거나 행 이름이 틀려도 **크래시도 컴파일 에러도 없다.** DataTable 은 못 찾은 행을
// nullptr 로 돌려줄 뿐이고, 그 뒤 증상은 "출발 버튼이 안 먹는다" 나 "목표를 채웠는데
// 판이 안 끝난다" 하나뿐이다. 원인까지 가려면 로그를 열어야 한다.
//
// `.ini` 배열이었을 때보다 조용해졌다 — 그때는 줄 문법이 틀리면 배열이 통째로 비어서
// 적어도 "전부 안 된다" 로 드러났는데, 표는 행 하나만 조용히 빠질 수 있다.
// 그래서 행마다 필수값을 여기서 못박는다.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSiteCatalogTest,
	"HeavyHanded.Heist.Travel.SiteCatalogResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSiteCatalogTest::RunTest(const FString& Parameters)
{
	const UHeistSettings* Settings = UHeistSettings::Get();

	const UDataTable* Table = Settings->GetSiteCatalog();
	if (!TestNotNull(TEXT("SiteCatalog 이 지정돼 있어야 한다 — "
						  "Project Settings → Game → Heist → Site Catalog"), Table))
	{
		return false;
	}

	if (!TestFalse(TEXT("DT_SiteCatalog 에 행이 하나도 없다"), Table->GetRowMap().IsEmpty()))
	{
		return false;
	}

	// ── 행마다 필수값 ──
	//
	// 하나라도 비면 그 장소만 조용히 못 쓰게 된다. 나머지 장소는 멀쩡해서 더 늦게 발견된다.
	int32 NamedRowNum = 0;
	TSet<int32> SeenOrders;

	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FHeistSiteRow* Row = reinterpret_cast<const FHeistSiteRow*>(Pair.Value);
		if (!Row)
		{
			continue;
		}

		const FString RowName = Pair.Key.ToString();

		// 행 이름이 곧 장소 태그다 (DT_NoiseProfiles 와 같은 약속).
		// 오타가 나면 FindSite 가 영영 못 찾고, 그 장소는 목록에서 사라진다
		const bool bNameIsTag = FGameplayTag::RequestGameplayTag(Pair.Key, /*ErrorIfNotFound=*/false).IsValid();
		TestTrue(FString::Printf(TEXT("행 이름 %s 가 Config/Tags/Phase.ini 에 등록된 Site.* 여야 한다"), *RowName),
			bNameIsTag);

		if (bNameIsTag)
		{
			++NamedRowNum;
		}

		TestFalse(FString::Printf(TEXT("%s 의 레벨이 지정되지 않았다 — 출발이 막힌다"), *RowName),
			Row->Level.IsNull());

		// 0 이면 AHeistGameState::IsTargetReached() 가 영영 false 라 성공 판정이 안 난다
		TestTrue(FString::Printf(TEXT("%s 의 목표 금액이 0 보다 커야 한다"), *RowName),
			Row->TargetValue > 0);

		TestTrue(FString::Printf(TEXT("%s 의 작업 시간이 0 보다 커야 한다"), *RowName),
			Row->HeistSeconds > 0.f);

		// 0 이면 SetPhase 가 '카운트다운 없음' 으로 읽어 도주가 영영 안 끝난다.
		// AHeistGameMode 에 90초 폴백이 있지만 폴백은 사고를 막는 장치지 정상 상태가 아니다
		TestTrue(FString::Printf(TEXT("%s 의 도주 시간이 0 보다 커야 한다"), *RowName),
			Row->EscapeSeconds > 0.f);

		TestFalse(FString::Printf(TEXT("%s 의 표시 이름이 비어 있다 — 화면에 '작업 장소' 로 뜬다"), *RowName),
			Row->DisplayName.IsEmpty());

		// 순서가 겹치면 캠페인 진행이 이름순 타이브레이커로 갈린다.
		// 지금은 결정적이지만 의도한 순서는 아니다
		bool bDuplicateOrder = false;
		SeenOrders.Add(Row->CampaignOrder, &bDuplicateOrder);
		TestFalse(FString::Printf(TEXT("%s 의 CampaignOrder(%d) 가 다른 장소와 겹친다"),
				*RowName, Row->CampaignOrder),
			bDuplicateOrder);

		if (Row->Image.IsNull())
		{
			AddInfo(FString::Printf(TEXT("%s 에 그림이 없다 — 위젯이 이미지 칸을 숨긴다"), *RowName));
		}
	}

	// ── 캠페인 순서 ──
	const TArray<FGameplayTag> Order = Settings->GetSiteOrder();

	// 이름이 태그인 행은 전부 순서에 들어와야 한다. 하나가 빠지면 그 장소는
	// 영영 다음 목표가 되지 않고, 캠페인이 그 앞에서 끝난 것처럼 보인다
	TestEqual(TEXT("이름이 유효한 행은 전부 캠페인 순서에 들어와야 한다"),
		Order.Num(), NamedRowNum);

	// CampaignOrder 오름차순인가. 이게 틀리면 저택 대신 은행으로 출발한다
	int32 PrevOrder = TNumericLimits<int32>::Lowest();
	for (const FGameplayTag& Site : Order)
	{
		const FHeistSiteRow* Row = Settings->FindSite(Site);
		if (!Row)
		{
			AddError(FString::Printf(TEXT("순서에 있는 %s 를 FindSite 가 못 찾는다"), *Site.ToString()));
			continue;
		}

		TestTrue(FString::Printf(TEXT("%s 의 CampaignOrder(%d) 가 앞 장소(%d)보다 작지 않아야 한다"),
				*Site.ToString(), Row->CampaignOrder, PrevOrder),
			Row->CampaignOrder >= PrevOrder);

		PrevOrder = Row->CampaignOrder;
	}

	// ── 첫 장소는 반드시 갈 수 있어야 한다 ──
	//
	// 기획서 2장의 시작점이다. 여기가 막히면 코어 루프 전체를 플레이해 볼 수 없다.
	// 장소 이름을 박지 않는 것은 순서를 CampaignOrder 가 정하기 때문이다 —
	// 여기에 Site.Mansion 을 적어 두면 표의 순서를 바꿨을 때 테스트가 거짓으로 통과한다
	if (TestFalse(TEXT("캠페인 순서가 비어 있다 — 갈 수 있는 장소가 없다"), Order.IsEmpty()))
	{
		const FSoftObjectPath FirstLevel = Settings->GetSiteLevel(Order[0]);

		TestFalse(FString::Printf(TEXT("첫 장소 %s 의 레벨이 등록돼 있어야 한다"), *Order[0].ToString()),
			FirstLevel.IsNull());
		TestFalse(TEXT("첫 장소로 출발 URL 을 만들 수 있어야 한다"),
			HeistTravel::BuildTravelURL(FirstLevel, 2).IsEmpty());
	}

	return true;
}


// ──────────────────────────────────────────────────────────────
// 장소별 진입점 목록 (FHeistSiteRow::Entries)
//
// 은신처에는 작업 레벨이 로드돼 있지 않아 진입점을 레벨에 물어볼 수 없다. 그래서 이 표가
// 선택 UI 의 유일한 근거다. 틀려도 게임은 안 죽는다 — HeistEntryGate 가 기본 진입점으로
// 폴백하기 때문이다. 그래서 증상이 "왜 고른 데서 안 나오지" 하나뿐이고 로그를 열어야 보인다.
//
// 실제로 한 번 겪은 경로다. 장소가 바뀌었는데 진입점 목록이 저택 것 그대로라서,
// 박물관으로 가면서 Entry.Mansion.* 을 고르고 있었다.
//
// **레벨에 배치된 AHeistEntryPoint 는 여기서 검사하지 못한다.** 테스트는 레벨을 로드하지
// 않는다 — 그쪽은 AHeistEntryPoint::BeginPlay 의 런타임 경고가 맡는다.
// ──────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistSiteEntriesTest,
	"HeavyHanded.Heist.Travel.SiteEntriesResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistSiteEntriesTest::RunTest(const FString& Parameters)
{
	const UHeistSettings* Settings = UHeistSettings::Get();

	// 행 맵을 직접 돌지 않는다 — 태그 목록으로 돌면 캐스팅이 필요 없고,
	// 이름이 태그가 아닌 행(SiteCatalogResolve 가 이미 잡는다)이 저절로 빠진다
	const TArray<FGameplayTag> Sites = Settings->GetSiteOrder();

	if (!TestFalse(TEXT("DT_SiteCatalog 에 장소가 없다 — 진입점을 검사할 대상이 없다"),
			Sites.IsEmpty()))
	{
		return false;
	}

	// ── 각 장소의 진입점이 제 모양인가 ──
	for (const FGameplayTag& SiteTag : Sites)
	{
		const FString SiteName = SiteTag.ToString();
		const TArray<FHeistEntryOption>& Entries = Settings->GetSiteEntries(SiteTag);

		if (Entries.IsEmpty())
		{
			// 실패가 아니다 — 맵이 아직 없는 장소는 비어 있는 것이 맞다.
			// 다만 조용히 넘어가면 "채우는 걸 잊은 것" 과 구분되지 않아서 남긴다
			AddInfo(FString::Printf(TEXT("%s 에 진입점이 없다 — 선택 UI 가 숨겨진다"), *SiteName));
			continue;
		}

		TSet<FGameplayTag> Seen;
		for (const FHeistEntryOption& Option : Entries)
		{
			TestTrue(FString::Printf(TEXT("%s 의 진입점 태그가 Config/Tags/Phase.ini 에 등록돼 있어야 한다"),
					*SiteName),
				Option.EntryTag.IsValid());

			// Site.* 나 Phase.* 를 잘못 고른 행을 잡는다. 태그 피커의 Categories 메타가
			// 에디터에서 걸러 주지만, 표를 CSV 로 가져오는 경로에는 그 방어가 없다
			TestTrue(FString::Printf(TEXT("%s 의 %s 는 Entry 루트 아래여야 한다"),
					*SiteName, *Option.EntryTag.ToString()),
				Option.EntryTag.MatchesTag(HHTags::Entry));

			// 같은 태그가 두 줄 있으면 선택 UI 에 버튼이 두 개 뜨고,
			// 둘 중 어느 쪽을 눌러도 같은 자리에서 시작한다. 고른 사람은 그걸 버그로 인지하지 못한다
			bool bAlreadySeen = false;
			Seen.Add(Option.EntryTag, &bAlreadySeen);
			TestFalse(FString::Printf(TEXT("%s 에 %s 가 두 번 등록돼 있다"),
					*SiteName, *Option.EntryTag.ToString()),
				bAlreadySeen);

			// 이름이 비면 선택 UI 가 태그 문자열을 그대로 보여준다 —
			// 플레이어에게 "Entry.Mansion.StartPoint1" 이 뜬다
			TestFalse(FString::Printf(TEXT("%s 의 %s 에 표시 이름이 없다"),
					*SiteName, *Option.EntryTag.ToString()),
				Option.DisplayName.IsEmpty());

			// 등록해 놓고 조회가 안 되면 서버 검증이 정상 선택을 거부한다
			TestTrue(FString::Printf(TEXT("%s 의 %s 를 IsEntryRegistered 가 찾아야 한다"),
					*SiteName, *Option.EntryTag.ToString()),
				Settings->IsEntryRegistered(SiteTag, Option.EntryTag));

			if (Option.Image.IsNull())
			{
				AddInfo(FString::Printf(TEXT("%s 의 %s 에 그림이 없다 — 위젯이 이미지 칸을 숨긴다"),
					*SiteName, *Option.EntryTag.ToString()));
			}
		}
	}

	// ── 진입점은 자기 장소에만 속한다 ──
	//
	// 이 파일에서 가장 중요한 블록이다. 저택 진입점을 들고 박물관으로 떠나던 그 버그를
	// 데이터 단계에서 잡는다. 태그 이름을 쪼개 보지 않는 것은, 그러면 "Entry.<장소>" 라는
	// 이름 규칙을 테스트가 한 벌 더 들고 있게 되기 때문이다 — 규칙이 두 곳이 된다
	for (const FGameplayTag& SiteTag : Sites)
	{
		for (const FGameplayTag& OtherTag : Sites)
		{
			if (OtherTag == SiteTag)
			{
				continue;
			}

			for (const FHeistEntryOption& Option : Settings->GetSiteEntries(OtherTag))
			{
				TestFalse(FString::Printf(TEXT("%s 의 진입점 %s 가 %s 에도 등록돼 있다"),
						*OtherTag.ToString(), *Option.EntryTag.ToString(), *SiteTag.ToString()),
					Settings->IsEntryRegistered(SiteTag, Option.EntryTag));
			}
		}
	}

	// ── 등록되지 않은 장소는 빈 배열이다 ──
	//
	// 폴백이 있으면 행을 빠뜨린 것을 "저택 진입점이 왜 여기 뜨지" 로 알게 된다.
	// Site.Bank 는 맵이 아직 없어 표에도 없다 — 등록되는 날 이 블록은 저절로 건너뛴다
	{
		const FGameplayTag Unknown =
			FGameplayTag::RequestGameplayTag(TEXT("Site.Bank"), /*ErrorIfNotFound=*/false);

		if (Unknown.IsValid() && !Settings->FindSite(Unknown))
		{
			TestTrue(TEXT("등록되지 않은 장소의 진입점 목록은 비어 있어야 한다"),
				Settings->GetSiteEntries(Unknown).IsEmpty());
			TestFalse(TEXT("등록되지 않은 장소에는 어떤 진입점도 속하지 않는다"),
				Settings->IsEntryRegistered(Unknown, FGameplayTag()));
		}
	}

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
