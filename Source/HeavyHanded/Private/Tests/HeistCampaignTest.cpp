#include "Misc/AutomationTest.h"

#include "Core/HeistCampaign.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameplayTag MakeSiteTag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistCampaignNextSiteTest,
	"HeavyHanded.Heist.Campaign.NextSiteIsFirstUnclearedInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistCampaignNextSiteTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Mansion = MakeSiteTag(TEXT("Site.Mansion"));
	const FGameplayTag Museum = MakeSiteTag(TEXT("Site.Museum"));
	const FGameplayTag Bank = MakeSiteTag(TEXT("Site.Bank"));

	TestTrue(TEXT("Site.Mansion 태그 등록 필요"), Mansion.IsValid());
	TestTrue(TEXT("Site.Museum 태그 등록 필요"), Museum.IsValid());
	TestTrue(TEXT("Site.Bank 태그 등록 필요"), Bank.IsValid());

	const TArray<FGameplayTag> Order = { Mansion, Museum, Bank };

	{
		const TArray<FGameplayTag> Cleared;
		TestEqual(TEXT("첫 장소"),
			HeistCampaign::GetNextSite(Order, Cleared), Mansion);
	}

	{
		const TArray<FGameplayTag> Cleared = { Mansion };
		TestEqual(TEXT("저택 통과, 다음 장소"),
			HeistCampaign::GetNextSite(Order, Cleared), Museum);
	}

	{
		const TArray<FGameplayTag> Cleared = { Mansion };
		TestEqual(TEXT("박물관에서 실패했을 경우"),
			HeistCampaign::GetNextSite(Order, Cleared), Museum);
	}

	{
		const TArray<FGameplayTag> Cleared = { Mansion, Museum, Bank };
		TestFalse(TEXT("모두 통과"),
			HeistCampaign::GetNextSite(Order, Cleared).IsValid());
	}

	{
		const TArray<FGameplayTag> Cleared = { Bank, Mansion };
		TestEqual(TEXT("깬 순서가 달라도 박물관인지"),
			HeistCampaign::GetNextSite(Order, Cleared), Museum);
	}

	{
		const TArray<FGameplayTag> Cleared = { Mansion, MakeSiteTag(TEXT("Site.Mansion.Garden")) };
		TestEqual(TEXT("순서에 없는 태그는 제외"),
			HeistCampaign::GetProgress(Order, Cleared), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeistCampaignCompleteTest,
	"HeavyHanded.Heist.Campaign.CompleteRequiresEveryRegisteredSite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHeistCampaignCompleteTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Mansion = MakeSiteTag(TEXT("Site.Mansion"));
	const FGameplayTag Museum = MakeSiteTag(TEXT("Site.Museum"));

	const TArray<FGameplayTag> Order = { Mansion, Museum };

	TestFalse(TEXT("등록된 장소 하나만 통과"),
		HeistCampaign::IsComplete(Order, { Mansion }));

	TestTrue(TEXT("등록된 장소 모두 통과"),
		HeistCampaign::IsComplete(Order, { Mansion, Museum }));

	TestFalse(TEXT("순서가 비어있으면 미완료"),
		HeistCampaign::IsComplete({}, {}));

	TestFalse(TEXT("순서가 무효 태그일 경우 미완료"),
		HeistCampaign::IsComplete({ FGameplayTag() }, {}));

	TestTrue(TEXT("장소가 하나뿐인 경우"),
		HeistCampaign::IsComplete({ Mansion }, { Mansion }));

	return true;
}

#endif
