#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "UI/Common/ToggleTypes.h"
#include "ToggleStateWidgets.generated.h"

UCLASS()
class HEAVYHANDED_API UToggleStateImage : public UImage, public IToggleStateListener
{
	GENERATED_BODY()

public:
	virtual void OnToggleVisualStateChanged_Implementation(EHHToggleVisualState NewState, bool bInstant) override;
	virtual bool TickToggleTransition(float DeltaTime) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "Toggle State")
	bool bSwapImage = false;

	UPROPERTY(EditAnywhere, Category = "Toggle State", meta = (EditCondition = "bSwapImage"))
	FHHToggleStateTextures Textures;

	UPROPERTY(EditAnywhere, Category = "Toggle State")
	bool bTintColor = false;

	UPROPERTY(EditAnywhere, Category = "Toggle State", meta = (EditCondition = "bTintColor"))
	FHHToggleStateColors Colors;

	UPROPERTY(EditAnywhere, Category = "Toggle State")
	bool bFade = true;

	UPROPERTY(EditAnywhere, Category = "Toggle State", meta = (EditCondition = "bFade", ClampMin = "0.01", Units = "s"))
	float FadeDuration = 0.1f;

private:
	void ApplyTint(const FLinearColor& Tint, float AlphaScale);

	FHHToggleFade Fade;
	FLinearColor BaseColor = FLinearColor::White;
	FLinearColor CurrentColor = FLinearColor::White;
	FLinearColor FromColor = FLinearColor::White;
	FLinearColor ToColor = FLinearColor::White;
	TWeakObjectPtr<UTexture2D> PendingTexture;
	bool bSwapPending = false;
	bool bInitialized = false;
};

UCLASS()
class HEAVYHANDED_API UToggleStateText : public UTextBlock, public IToggleStateListener
{
	GENERATED_BODY()

public:
	virtual void OnToggleVisualStateChanged_Implementation(EHHToggleVisualState NewState, bool bInstant) override;
	virtual bool TickToggleTransition(float DeltaTime) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "Toggle State")
	FHHToggleStateColors Colors;

	UPROPERTY(EditAnywhere, Category = "Toggle State")
	bool bFade = true;

	UPROPERTY(EditAnywhere, Category = "Toggle State", meta = (EditCondition = "bFade", ClampMin = "0.01", Units = "s"))
	float FadeDuration = 0.1f;

private:
	FHHToggleFade Fade;
	FLinearColor CurrentColor = FLinearColor::White;
	FLinearColor FromColor = FLinearColor::White;
	FLinearColor ToColor = FLinearColor::White;
};

UCLASS()
class HEAVYHANDED_API UToggleStateVisibility : public UOverlay, public IToggleStateListener
{
	GENERATED_BODY()

public:
	virtual void OnToggleVisualStateChanged_Implementation(EHHToggleVisualState NewState, bool bInstant) override;
	virtual bool TickToggleTransition(float DeltaTime) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "Toggle State")
	FHHToggleStateFlags VisibleStates;

	UPROPERTY(EditAnywhere, Category = "Toggle State")
	bool bCollapseWhenHidden = false;

	UPROPERTY(EditAnywhere, Category = "Toggle State")
	bool bFade = true;

	UPROPERTY(EditAnywhere, Category = "Toggle State", meta = (EditCondition = "bFade", ClampMin = "0.01", Units = "s"))
	float FadeDuration = 0.1f;

private:
	ESlateVisibility GetHiddenVisibility() const;
	bool IsShown() const;

	FHHToggleFade Fade;
	float FromOpacity = 1.f;
	bool bTargetShown = false;
};
