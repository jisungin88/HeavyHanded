#include "UI/HeavyUIText.h"

#include "GameFramework/Actor.h"

#include "Loot/LootBase.h"

#define LOCTEXT_NAMESPACE "HeavyUI"

namespace
{
	/**
	 * [임시] 내부 이름을 사람이 읽을 만하게 다듬는다.
	 *
	 * "BP_Loot_Vase_C" → "Vase"
	 *
	 * [왜 임시인가] 이름의 진리원은 DT_LootCatalog 하나여야 한다. 여기서 만든 이름은
	 *   표에 행이 없는 물건에만 붙는 대타다. 표가 채워지면 이 함수는 지운다.
	 *
	 *   ALootBase::GetDisplayName() 주석이 "GetName() 을 그대로 돌려주지 말 것" 이라고
	 *   경고하는 이유가 이것이다 — 안 다듬으면 "BP_Loot_Fragile_C_0" 이 화면에 그대로 뜬다.
	 */
	FText CleanUpInternalName(const FString& RawName)
	{
		FString Name = RawName;

		Name.RemoveFromEnd(TEXT("_C"));
		Name.RemoveFromStart(TEXT("BP_"));
		Name.RemoveFromStart(TEXT("Loot_"));
		Name.ReplaceInline(TEXT("_"), TEXT(" "));
		Name.TrimStartAndEndInline();

		return Name.IsEmpty()
			? LOCTEXT("LootNameUnknown", "물건")
			: FText::FromString(Name);
	}
}

FText HeavyUIText::LootName(const AActor* LootActor)
{
	if (!IsValid(LootActor))
	{
		return LOCTEXT("LootNameUnknown", "물건");
	}

	if (const ALootBase* Loot = Cast<ALootBase>(LootActor))
	{
		// 표에 행이 지정돼 있으면 액터가 만들어질 때 채워져 있다
		const FText& Name = Loot->GetDisplayName();
		if (!Name.IsEmpty())
		{
			return Name;
		}

		// 행 이름은 표의 열쇠라서 클래스 이름보다 낫다 ("Vase_Fragile" 같은 값)
		const FName RowName = Loot->GetLootRowName();
		if (!RowName.IsNone())
		{
			return CleanUpInternalName(RowName.ToString());
		}
	}

	return CleanUpInternalName(LootActor->GetClass()->GetName());
}

FText HeavyUIText::LootName(TSubclassOf<ALootBase> LootClass)
{
	if (!LootClass)
	{
		return LOCTEXT("LootNameUnknown", "물건");
	}

	if (const ALootBase* CDO = LootClass.GetDefaultObject())
	{
		// 표시 이름은 PostInitializeComponents 에서 채워지므로 CDO 에는 보통 비어 있다.
		// 그래도 보는 이유는 나중에 노획물 파트가 기본값으로 넣을 수 있기 때문이다
		const FText& Name = CDO->GetDisplayName();
		if (!Name.IsEmpty())
		{
			return Name;
		}

		const FName RowName = CDO->GetLootRowName();
		if (!RowName.IsNone())
		{
			return CleanUpInternalName(RowName.ToString());
		}
	}

	return CleanUpInternalName(LootClass->GetName());
}

FText HeavyUIText::Duration(float Seconds)
{
	// 올림이라 마지막 1초가 0:00 으로 먼저 넘어가지 않는다
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));

	// 초는 항상 두 자리다 — "6:5" 가 아니라 "6:05".
	// 자릿수가 오갈 때마다 글자 폭이 흔들리면 시선이 그쪽으로 끌린다
	FNumberFormattingOptions SecondsFormat;
	SecondsFormat.MinimumIntegralDigits = 2;

	return FText::Format(LOCTEXT("DurationFormat", "{0}:{1}"),
						 FText::AsNumber(Total / 60),
						 FText::AsNumber(Total % 60, &SecondsFormat));
}

FText HeavyUIText::Money(int32 Amount)
{
	return FText::Format(LOCTEXT("MoneyFormat", "${0}"), FText::AsNumber(Amount));
}

#undef LOCTEXT_NAMESPACE
