#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "UObject/Interface.h"
#include "ToggleTypes.generated.h"

UENUM(BlueprintType)
enum class EHHToggleVisualState : uint8
{
	Normal,
	Hovered,
	Pressed,
	Selected,
	SelectedHovered,
	Disabled,
};
USTRUCT(BlueprintType)
struct FHHToggleStateColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	FLinearColor Normal = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	FLinearColor Hovered = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	FLinearColor Pressed = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	FLinearColor Selected = FLinearColor::White;

	/** 선택된 토글에 호버 컬러 사용여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bOverrideSelectedHovered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle", meta = (EditCondition = "bOverrideSelectedHovered"))
	FLinearColor SelectedHovered = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	FLinearColor Disabled = FLinearColor::White;

	FLinearColor Get(EHHToggleVisualState State) const
	{
		switch (State)
		{
		case EHHToggleVisualState::Hovered:			return Hovered;
		case EHHToggleVisualState::Pressed:			return Pressed;
		case EHHToggleVisualState::Selected:		return Selected;
		case EHHToggleVisualState::SelectedHovered: return bOverrideSelectedHovered ? SelectedHovered : Selected;
		case EHHToggleVisualState::Disabled:		return Disabled;
		default:									return Normal;
		}
	}
};

USTRUCT(BlueprintType)
struct FHHToggleStateTextures
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TObjectPtr<UTexture2D> Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TObjectPtr<UTexture2D> Hovered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TObjectPtr<UTexture2D> Pressed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TObjectPtr<UTexture2D> Selected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TObjectPtr<UTexture2D> SelectedHovered;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	TObjectPtr<UTexture2D> Disabled;

	UTexture2D* Get(EHHToggleVisualState State) const
	{
		UTexture2D* Found = nullptr;
		switch (State)
		{
		case EHHToggleVisualState::Hovered:			Found = Hovered;			break;
		case EHHToggleVisualState::Pressed:			Found = Pressed;			break;
		case EHHToggleVisualState::Selected:		Found = Selected;		break;
		case EHHToggleVisualState::SelectedHovered: Found = SelectedHovered ? SelectedHovered.Get() : Selected.Get();	break;
		case EHHToggleVisualState::Disabled:		Found = Disabled;		break;
		default: break;
		}
		return Found ? Found : Normal.Get();
	}
};

USTRUCT(BlueprintType)
struct FHHToggleStateFlags
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bNormal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bHovered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bPressed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bSelected = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bSelectedHovered = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toggle")
	bool bDisabled = false;

	bool Get(EHHToggleVisualState State) const
	{
		switch (State)
		{
		case EHHToggleVisualState::Hovered:			return bHovered;
		case EHHToggleVisualState::Pressed:			return bPressed;
		case EHHToggleVisualState::Selected:		return bSelected;
		case EHHToggleVisualState::SelectedHovered: return bSelectedHovered;
		case EHHToggleVisualState::Disabled:		return bDisabled;
		default:									return bNormal;
		}
	}
};

struct FHHToggleFade
{
	void Start(float InDuration) { Duration = FMath::Max(InDuration, 0.f); Elapsed = 0.f;}
	void Stop() { Duration = 0.f; Elapsed = 0.f; }
	bool IsActive() const { return Elapsed < Duration; }

	float Advance(float DeltaTime)
	{
		Elapsed = FMath::Min(Elapsed + DeltaTime, Duration);
		return Duration > 0.f ? Elapsed / Duration : 1.f;
	}

private:
	float Duration = 0.f;
	float Elapsed = 0.f;
};

UINTERFACE(Blueprintable)
class UToggleStateListener : public UInterface
{
	GENERATED_BODY()
};

class HEAVYHANDED_API IToggleStateListener
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Toggle")
	void OnToggleVisualStateChanged(EHHToggleVisualState NewState, bool bInstant);

	virtual bool TickToggleTransition(float DeltaTime) { return false; }
};
