#include "UI/Common/ToggleButtonGroupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlot.h"
#include "UI/Common/ToggleButtonWidget.h"
#include "UI/HeavyUILog.h"

void UToggleButtonGroupWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshToggles();
}

void UToggleButtonGroupWidget::NativeDestruct()
{
	ClearToggles();
	Super::NativeDestruct();
}

void UToggleButtonGroupWidget::RefreshToggles()
{
	ClearToggles();

	UWidget* Content = Slot_Toggles ? Slot_Toggles->GetContent() : nullptr;
	if (!Content)
	{
		return;
	}

	// 자신의 자녀들에서 UToggleButtonWidget를 찾고 반영
	UWidgetTree::ForWidgetAndChildren(Content, [this](UWidget* Widget)
	{
		if (UToggleButtonWidget* Toggle = Cast<UToggleButtonWidget>(Widget))
		{
			Toggle->SetGroup(this);
			Toggles.Add(Toggle);
		}
	});

	// 켜진 것이 하나도 없거나 여러개일 경우 첫번째 것으로 킨다.
	UToggleButtonWidget* Keep = GetSelectedToggle();
	if (!Keep && !bAllowSwitchOff && Toggles.Num() > 0)
	{
		Keep = Toggles[0];
	}

	for (UToggleButtonWidget* Toggle : Toggles)
	{
		Toggle->SetIsOn(Toggle == Keep, false, true);
	}
}

void UToggleButtonGroupWidget::SelectToggle(UToggleButtonWidget* Toggle, bool bBroadcast)
{
	if (!Toggle && !bAllowSwitchOff)
	{
		return;
	}

	if (Toggle && !Toggles.Contains(Toggle))
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("%s 는 그룹 %s 소속이 아닙니다. 슬롯 밖에 있거나 RefreshToggles 가 필요합니다."),
		       *GetNameSafe(Toggle), *GetName());
		return;
	}

	if (GetSelectedToggle() == Toggle)
	{
		return;
	}

	for (UToggleButtonWidget* Other : Toggles)
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

bool UToggleButtonGroupWidget::SelectById(FName Id, bool bBroadcast)
{
	for (UToggleButtonWidget* Toggle : Toggles)
	{
		if (Toggle->GetToggleId() == Id)
		{
			SelectToggle(Toggle, bBroadcast);
			return true;
		}
	}
	return false;
}

UToggleButtonWidget* UToggleButtonGroupWidget::GetSelectedToggle() const
{
	for (UToggleButtonWidget* Toggle : Toggles)
	{
		if (Toggle->IsOn())
		{
			return Toggle;
		}
	}
	return nullptr;
}

FName UToggleButtonGroupWidget::GetSelectedId() const
{
	const UToggleButtonWidget* Selected = GetSelectedToggle();
	return Selected ? Selected->GetToggleId() : NAME_None;
}

void UToggleButtonGroupWidget::HandleToggleClicked(UToggleButtonWidget* Toggle)
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

void UToggleButtonGroupWidget::ClearToggles()
{
	for (UToggleButtonWidget* Toggle : Toggles)
	{
		if (Toggle && Toggle->GetGroup() == this)
		{
			Toggle->SetGroup(nullptr);
		}
	}
	Toggles.Reset();
}
