#include "UI/Common/ToggleButtonWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "UI/Common/ToggleButtonGroupWidget.h"

void UToggleButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Btn_Toggle)
	{
		Btn_Toggle->OnClicked.AddDynamic(this, &UToggleButtonWidget::HandleClicked);
		Btn_Toggle->OnHovered.AddDynamic(this, &UToggleButtonWidget::HandleHovered);
		Btn_Toggle->OnUnhovered.AddDynamic(this, &UToggleButtonWidget::HandleUnhovered);
		Btn_Toggle->OnPressed.AddDynamic(this, &UToggleButtonWidget::HandlePressed);
		Btn_Toggle->OnReleased.AddDynamic(this, &UToggleButtonWidget::HandleReleased);
	}
}

void UToggleButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	CollectListeners();

	if (Btn_Toggle)
	{
		Btn_Toggle->SetIsEnabled(bInteractable);
	}

	NotifyVisualState(true);
	OnStateChanged(bIsOn);
}

void UToggleButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bTransitionRunning)
	{
		return;
	}

	bool bAnyRunning = false;
	for (UWidget* Widget : Listeners)
	{
		if (IToggleStateListener* Listener = Cast<IToggleStateListener>(Widget))
		{
			bAnyRunning |= Listener->TickToggleTransition(InDeltaTime);
		}
	}
	bTransitionRunning = bAnyRunning;
}

void UToggleButtonWidget::SetIsOn(bool bNewIsOn, bool bBroadcast, bool bInstant)
{
	if (bIsOn == bNewIsOn)
	{
		return;
	}

	bIsOn = bNewIsOn;
	NotifyVisualState(bInstant);
	OnStateChanged(bIsOn);

	if (bBroadcast)
	{
		OnToggled.Broadcast(this, bIsOn);
	}
}

void UToggleButtonWidget::SetToggleInteractable(bool bNewInteractable)
{
	if (bInteractable == bNewInteractable)
	{
		return;
	}

	bInteractable = bNewInteractable;
	if (Btn_Toggle)
	{
		Btn_Toggle->SetIsEnabled(bInteractable);
	}

	bHovered = false;
	bPressed = false;
	NotifyVisualState(false);
}

EHHToggleVisualState UToggleButtonWidget::GetVisualState() const
{
	if (!bInteractable)
	{
		return EHHToggleVisualState::Disabled;
	}
	if (bIsOn)
	{
		return bHovered ? EHHToggleVisualState::SelectedHovered : EHHToggleVisualState::Selected;
	}
	if (bPressed)
	{
		return EHHToggleVisualState::Pressed;
	}
	return bHovered ? EHHToggleVisualState::Hovered : EHHToggleVisualState::Normal;
}

void UToggleButtonWidget::CollectListeners()
{
	Listeners.Reset();
	if (!WidgetTree)
	{
		return;
	}

	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (Widget && Widget->Implements<UToggleStateListener>())
		{
			Listeners.Add(Widget);
		}
	});
}

void UToggleButtonWidget::NotifyVisualState(bool bInstant)
{
	const EHHToggleVisualState NewState = GetVisualState();
	if (!bInstant && NewState == LastVisualState)
	{
		return;
	}
	LastVisualState = NewState;

	for (UWidget* Widget : Listeners)
	{
		if (IsValid(Widget))
		{
			IToggleStateListener::Execute_OnToggleVisualStateChanged(Widget, NewState, bInstant);
		}
	}

	OnVisualStateChanged(NewState, bInstant);
	if (!bInstant)
	{
		bTransitionRunning = true;
	}
}

void UToggleButtonWidget::HandleClicked()
{
	if (UToggleButtonGroupWidget* MyGroup = Group.Get())
	{
		MyGroup->HandleToggleClicked(this);
		return;
	}
	SetIsOn(!bIsOn, true);
}

void UToggleButtonWidget::HandleHovered()
{
	bHovered = true;
	NotifyVisualState(false);
}

void UToggleButtonWidget::HandleUnhovered()
{
	bHovered = false;
	bPressed = false;
	NotifyVisualState(false);
}

void UToggleButtonWidget::HandlePressed()
{
	bPressed = true;
	NotifyVisualState(false);
}

void UToggleButtonWidget::HandleReleased()
{
	bPressed = false;
	NotifyVisualState(false);
}

