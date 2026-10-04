#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Common/ToggleTypes.h"
#include "ToggleButtonWidget.generated.h"

class UButton;
class UToggleButtonGroupWidget;
class UToggleButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToggleStateChanged, UToggleButtonWidget*, Toggle, bool, bIsOn);

UCLASS(Abstract)
class HEAVYHANDED_API UToggleButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI|Toggle")
	void SetIsOn(bool bNewIsOn, bool bBroadcast = false, bool bInstant = false);

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	bool IsOn() const { return bIsOn; }

	UFUNCTION(BlueprintCallable, Category = "UI|Toggle")
	void SetToggleInteractable(bool bNewInteractable);

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	bool IsToggleInteractable() const { return bInteractable; }

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	EHHToggleVisualState GetVisualState() const;

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	FName GetToggleId() const { return ToggleId; }

	UFUNCTION(BlueprintCallable, Category = "UI|Toggle")
	void SetToggleId(FName NewId) { ToggleId = NewId; }

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	UToggleButtonGroupWidget* GetGroup() const { return Group.Get(); }

	void SetGroup(UToggleButtonGroupWidget* InGroup) { Group = InGroup; }

	UPROPERTY(BlueprintAssignable, Category = "UI|Toggle")
	FOnToggleStateChanged OnToggled;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Toggle")
	void OnStateChanged(bool bNewIsOn);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Toggle")
	void OnVisualStateChanged(EHHToggleVisualState NewState, bool bInstant);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toggle")
	FName ToggleId;

	UPROPERTY(EditAnywhere, Category = "Toggle")
	bool bIsOn = false;

	UPROPERTY(EditAnywhere, Category = "Toggle")
	bool bInteractable = true;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Toggle", meta = (BindWidget))
	TObjectPtr<UButton> Btn_Toggle;

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> Listeners;

	TWeakObjectPtr<UToggleButtonGroupWidget> Group;
	EHHToggleVisualState LastVisualState = EHHToggleVisualState::Normal;
	bool bHovered = false;
	bool bPressed = false;
	bool bTransitionRunning = false;
};
