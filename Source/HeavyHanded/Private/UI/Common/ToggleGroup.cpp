#include "UI/Common/ToggleGroup.h"

#include "Blueprint/WidgetTree.h"
#include "UI/Common/ToggleButton.h"
#include "UI/HeavyUILog.h"

#define LOCTEXT_NAMESPACE "InyToggle"


void UToggleGroup::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	RefreshToggles();
}

void UToggleGroup::RefreshToggles()
{
	ClearToggles();

	UWidgetTree::ForWidgetAndChildren(this, [this](UWidget* Widget)
	{
		if (UToggleButton* Toggle = Cast<UToggleButton>(Widget))
		{
			Toggle->SetGroup(this);
			Toggles.Add(Toggle);
		}
	});

	UToggleButton* Keep = GetSelectedToggle();
	if (!Keep && !bAllowSwitchOff && Toggles.Num() > 0)
	{
		Keep = Toggles[0];
	}

	for (UToggleButton* Toggle : Toggles)
	{
		// 화면 열 때 정리하는 것이라 연출이 없음.
		Toggle->SetIsOn(Toggle == Keep, false, true);
	}
}

void UToggleGroup::SelectToggle(UToggleButton* Toggle, bool bBroadcast)
{
	if (!Toggle && !bAllowSwitchOff)
	{
		return;
	}

	if (Toggle && !Toggles.Contains(Toggle))
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("%s 는 그룹 %s 소속이 아닙니다. 그룹 밖에 있거나 RefreshToggles 가 필요합니다."),
			*GetNameSafe(Toggle), *GetName());
		return;
	}

	if (GetSelectedToggle() == Toggle)
	{
		return;
	}

	for (UToggleButton* Other : Toggles)
	{
		if (Other != Toggle)
		{
			Other->SetIsOn(false, bBroadcast);
		}
	}

	if (Toggle)
	{
		Toggle->SetIsOn(true, bBroadcast);
	}

	if (bBroadcast)
	{
		OnSelectionChanged.Broadcast(Toggle, Toggle ? Toggle->GetToggleId() : NAME_None);
	}
}

bool UToggleGroup::SelectById(FName Id, bool bBroadcast)
{
	for (UToggleButton* Toggle : Toggles)
	{
		if (Toggle->GetToggleId() == Id)
		{
			SelectToggle(Toggle, bBroadcast);
			return true;
		}
	}
	return false;
}

UToggleButton* UToggleGroup::GetSelectedToggle() const
{
	for (UToggleButton* Toggle : Toggles)
	{
		if (Toggle->IsOn())
		{
			return Toggle;
		}
	}

	return nullptr;
}

FName UToggleGroup::GetSelectedId() const
{
	const UToggleButton* Selected = GetSelectedToggle();
	return Selected ? Selected->GetToggleId() : NAME_None;

}

void UToggleGroup::HandleToggleClicked(UToggleButton* Toggle)
{
	if (Toggle->IsOn())
	{
		if (bAllowSwitchOff)
		{
			SelectToggle(nullptr, true);
		}
		return;
	}

	SelectToggle(Toggle, true);
}

void UToggleGroup::ClearToggles()
{
	for (UToggleButton* Toggle : Toggles)
	{
		if (Toggle && Toggle->GetGroup() == this)
		{
			Toggle->SetGroup(nullptr);
		}
	}
	Toggles.Reset();
}

#if WITH_EDITOR
const FText UToggleGroup::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Toggles");
}
#endif

#undef LOCTEXT_NAMESPACE
