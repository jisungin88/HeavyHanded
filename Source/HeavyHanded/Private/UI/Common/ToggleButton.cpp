#include "UI/Common/ToggleButton.h"

#include "Blueprint/WidgetTree.h"
#include "UI/Common/ToggleGroup.h"

#define LOCTEXT_NAMESPACE "InyToggle"

UToggleButton::UToggleButton()
{
	FButtonStyle Style = GetStyle();
	Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.PressedPadding = Style.NormalPadding;
	SetStyle(Style);

	InitIsFocusable(false);
}

TSharedRef<SWidget> UToggleButton::RebuildWidget()
{
	TSharedRef<SWidget> Result = Super::RebuildWidget();

	OnClicked.AddUniqueDynamic(this, &UToggleButton::HandleClicked);
	OnHovered.AddUniqueDynamic(this, &UToggleButton::HandleHovered);
	OnUnhovered.AddUniqueDynamic(this, &UToggleButton::HandleUnhovered);
	OnPressed.AddUniqueDynamic(this, &UToggleButton::HandlePressed);
	OnReleased.AddUniqueDynamic(this, &UToggleButton::HandleReleased);

	return Result;
}

void UToggleButton::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	CollectListeners();
	NotifyVisualState(true);
}


void UToggleButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// 디자이너에서 bIsOn, IsEnabled를 바꾸면 미리보기에 반영
	NotifyVisualState(true);
}

void UToggleButton::ReleaseSlateResources(bool bReleaseChildren)
{
	StopTicking();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UToggleButton::BeginDestroy()
{
	StopTicking();
	Super::BeginDestroy();
}

void UToggleButton::SetIsOn(bool bNewIsOn, bool bBroadcast, bool bInstant)
{
	if (bIsOn == bNewIsOn)
	{
		return;
	}

	bIsOn = bNewIsOn;
	NotifyVisualState(bInstant);

	if (bBroadcast)
	{
		OnToggled.Broadcast(this, bIsOn);
	}
}

void UToggleButton::SetToggleInteractable(bool bInteractable)
{
	if (GetIsEnabled() == bInteractable)
	{
		return;
	}

	SetIsEnabled(bInteractable);

	bHovered = false;
	bPressed = false;
	NotifyVisualState(false);
}

EHHToggleVisualState UToggleButton::GetVisualState() const
{
	if (!GetIsEnabled())
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

void UToggleButton::CollectListeners()
{
	Listeners.Reset();

	UWidgetTree::ForWidgetAndChildren(this, [this](UWidget* Widget)
	{
		if (Widget && Widget != this && Widget->Implements<UToggleStateListener>())
		{
			Listeners.Add(Widget);
		}
	});
}

void UToggleButton::NotifyVisualState(bool bInstant)
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

	OnVisualStateChanged.Broadcast(this, NewState, bInstant);

	if (bInstant)
	{
		StopTicking();
	}
	else
	{
		StartTicking();
	}
}

void UToggleButton::StartTicking()
{
	if (TickHandle.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UToggleButton> WeakThis(this);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakThis](float DeltaTime)
		{
			UToggleButton* Self = WeakThis.Get();
			return Self && Self->TickTransitions(DeltaTime);
		}));
}

void UToggleButton::StopTicking()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
}

bool UToggleButton::TickTransitions(float DeltaTime)
{
	bool bAnyRunning = false;
	for (UWidget* Widget : Listeners)
	{
		if (IToggleStateListener* Listener = Cast<IToggleStateListener>(Widget))
		{
			bAnyRunning |= Listener->TickToggleTransition(DeltaTime);
		}
	}

	// false일 경우 티커가 빠지기에 핸들 비움
	if (!bAnyRunning)
	{
		TickHandle.Reset();
	}

	return bAnyRunning;
}

void UToggleButton::HandleClicked()
{
	if (UToggleGroup* MyGroup = Group.Get())
	{
		MyGroup->HandleToggleClicked(this);
		return;
	}
	SetIsOn(!bIsOn, true);
}

void UToggleButton::HandleHovered()
{
	bHovered = true;
	NotifyVisualState(false);
}

void UToggleButton::HandleUnhovered()
{
	bHovered = false;
	bPressed = false;
	NotifyVisualState(false);
}

void UToggleButton::HandlePressed()
{
	bPressed = true;
	NotifyVisualState(false);
}

void UToggleButton::HandleReleased()
{
	bPressed = false;
	NotifyVisualState(false);
}

#if WITH_EDITOR
const FText UToggleButton::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Toggle");
}
#endif

#undef LOCTEXT_NAMESPACE
