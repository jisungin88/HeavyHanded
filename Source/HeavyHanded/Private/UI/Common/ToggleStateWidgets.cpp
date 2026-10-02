#include "UI/Common/ToggleStateWidgets.h"

#define LOCTEXT_NAMESPACE "HeavyHandedToggle"

// --- Image

void UToggleStateImage::OnToggleVisualStateChanged_Implementation(EHHToggleVisualState NewState, bool bInstant)
{
	if (!bInitialized)
	{
		BaseColor = GetColorAndOpacity();
		CurrentColor = BaseColor;
		bInitialized = true;
	}

	FromColor = CurrentColor;
	ToColor = bTintColor ? Colors.Get(NewState) : BaseColor;

	UTexture2D* NewTexture = bSwapImage ? Textures.Get(NewState) : nullptr;
	const bool bTextureChanges = NewTexture && NewTexture != GetBrush().GetResourceObject();

	if (bInstant || !bFade)
	{
		if (bTextureChanges)
		{
			SetBrushFromTexture(NewTexture);
		}
		Fade.Stop();
		bSwapPending = false;
		CurrentColor = ToColor;
		ApplyTint(CurrentColor, 1.f);
		return;
	}

	PendingTexture = bTextureChanges ? NewTexture : nullptr;
	bSwapPending = bTextureChanges;
	Fade.Start(FadeDuration);
}

bool UToggleStateImage::TickToggleTransition(float DeltaTime)
{
	if (!Fade.IsActive())
	{
		return false;
	}

	const float Alpha = Fade.Advance(DeltaTime);
	CurrentColor = FMath::Lerp(FromColor, ToColor, Alpha);

	float CurVisibility = 1.f;
	if (bSwapPending)
	{
		if (Alpha >= 0.5f && PendingTexture.IsValid())
		{
			SetBrushFromTexture(PendingTexture.Get());
			PendingTexture.Reset();
		}
		CurVisibility = FMath::Abs(Alpha * 2.f - 1.f);
	}

	ApplyTint(CurrentColor, CurVisibility);

	if (!Fade.IsActive())
	{
		bSwapPending = false;
	}

	return Fade.IsActive();
}

void UToggleStateImage::ApplyTint(const FLinearColor& Tint, float AlphaScale)
{
	FLinearColor Color = Tint;
	Color.A *= AlphaScale;
	SetColorAndOpacity(Color);
}

#if WITH_EDITOR
const FText UToggleStateImage::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Toggle");
}
#endif


// --- Text

void UToggleStateText::OnToggleVisualStateChanged_Implementation(EHHToggleVisualState NewState, bool bInstant)
{
	FromColor = CurrentColor;
	ToColor = Colors.Get(NewState);

	if (bInstant || !bFade)
	{
		Fade.Stop();
		CurrentColor = ToColor;
		SetColorAndOpacity(FSlateColor(CurrentColor));
		return;
	}

	Fade.Start(FadeDuration);
}

bool UToggleStateText::TickToggleTransition(float DeltaTime)
{
	if (!Fade.IsActive())
	{
		return false;
	}

	CurrentColor = FMath::Lerp(FromColor, ToColor, Fade.Advance(DeltaTime));
	SetColorAndOpacity(FSlateColor(CurrentColor));
	return Fade.IsActive();
}

#if WITH_EDITOR
const FText UToggleStateText::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Toggle");
}
#endif


// --- Visibility

void UToggleStateVisibility::OnToggleVisualStateChanged_Implementation(EHHToggleVisualState NewState, bool bInstant)
{
	const bool bShow = VisibleStates.Get(NewState);

	if (bInstant || !bFade)
	{
		Fade.Stop();
		bTargetShown = bShow;
		SetRenderOpacity(1.f);
		SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : GetHiddenVisibility());
		return;
	}

	if (bShow == bTargetShown)
	{
		return;
	}

	FromOpacity = IsShown() ? GetRenderOpacity() : 0.f;
	bTargetShown = bShow;

	if (bShow)
	{
		SetRenderOpacity(FromOpacity);
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	Fade.Start(FadeDuration);
}

bool UToggleStateVisibility::TickToggleTransition(float DeltaTime)
{
	if (!Fade.IsActive())
	{
		return false;
	}

	const float Alpha = Fade.Advance(DeltaTime);
	SetRenderOpacity(FMath::Lerp(FromOpacity, bTargetShown ? 1.f : 0.f, Alpha));

	if (!Fade.IsActive() && !bTargetShown)
	{
		SetVisibility(GetHiddenVisibility());
		SetRenderOpacity(1.f);
	}

	return Fade.IsActive();
}

ESlateVisibility UToggleStateVisibility::GetHiddenVisibility() const
{
	return bCollapseWhenHidden ? ESlateVisibility::Collapsed : ESlateVisibility::Hidden;
}

bool UToggleStateVisibility::IsShown() const
{
	const ESlateVisibility Current = GetVisibility();
	return Current != ESlateVisibility::Hidden && Current != ESlateVisibility::Collapsed;
}

#if WITH_EDITOR
const FText UToggleStateVisibility::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Toggle");
}
#endif

#undef LOCTEXT_NAMESPACE
