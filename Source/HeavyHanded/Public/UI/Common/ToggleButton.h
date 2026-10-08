#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Containers/Ticker.h"
#include "UI/Common/ToggleTypes.h"
#include "ToggleButton.generated.h"

class UToggleButton;
class UToggleGroup;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToggleStateChanged, UToggleButton*, Toggle, bool, bIsOn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnToggleVisualStateChanged, UToggleButton*, Toggle, EHHToggleVisualState, NewState, bool, bInstant);

UCLASS()
class HEAVYHANDED_API UToggleButton : public UButton
{
	GENERATED_BODY()

public:
	UToggleButton();

	UFUNCTION(BlueprintCallable, Category = "Toggle")
	void SetIsOn(bool bNewIsOn, bool bBroadcast = false, bool bInstant = false);

	UFUNCTION(BlueprintPure, Category = "Toggle")
	bool IsOn() const { return bIsOn; }

	UFUNCTION(BlueprintCallable, Category = "Toggle")
	void SetToggleInteractable(bool bInteractable);

	UFUNCTION(BlueprintPure, Category = "Toggle")
	EHHToggleVisualState GetVisualState() const;

	UFUNCTION(BlueprintPure, Category = "Toggle")
	FName GetToggleId() const { return ToggleId; }

	UFUNCTION(BlueprintCallable, Category = "Toggle")
	void SetToggleId(FName NewId) { ToggleId = NewId; }

	UFUNCTION(BlueprintPure, Category = "Toggle")
	UToggleGroup* GetGroup() const { return Group.Get(); }

	void SetGroup(UToggleGroup* InGroup) { Group = InGroup; }

	UPROPERTY(BlueprintAssignable, Category = "Toggle|Event")
	FOnToggleStateChanged OnToggled;

	UPROPERTY(BlueprintAssignable, Category = "Toggle|Event")
	FOnToggleVisualStateChanged OnVisualStateChanged;

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnWidgetRebuilt() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toggle")
	FName ToggleId;

	UPROPERTY(EditAnywhere, Category = "Toggle")
	bool bIsOn = false;

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UFUNCTION()
	void HandlePressed();

	UFUNCTION()
	void HandleReleased();

	void CollectListeners();
	void NotifyVisualState(bool bInstant);

	void StartTicking();
	void StopTicking();
	bool TickTransitions(float DeltaTime);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> Listeners;

	TWeakObjectPtr<UToggleGroup> Group;
	FTSTicker::FDelegateHandle TickHandle;
	EHHToggleVisualState LastVisualState = EHHToggleVisualState::Normal;
	bool bHovered = false;
	bool bPressed = false;
};
