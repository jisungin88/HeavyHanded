#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToggleButtonGroupWidget.generated.h"

class UNamedSlot;
class UToggleButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToggleGroupChanged, UToggleButtonWidget*, Selected, FName, SelectedId);

UCLASS(Abstract)
class HEAVYHANDED_API UToggleButtonGroupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 슬롯 안의 토글을 찾는다 */
	UFUNCTION(BlueprintCallable, Category = "UI|Toggle")
	void RefreshToggles();

	UFUNCTION(BlueprintCallable, Category = "UI|Toggle")
	void SelectToggle(UToggleButtonWidget* Toggle, bool bBroadcast = false);

	UFUNCTION(BlueprintCallable, Category = "UI|Toggle")
	bool SelectById(FName Id, bool bBroadcast = false);

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	UToggleButtonWidget* GetSelectedToggle() const;

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	FName GetSelectedId() const;

	UFUNCTION(BlueprintPure, Category = "UI|Toggle")
	TArray<UToggleButtonWidget*> GetToggles() const { return Toggles; }

	UPROPERTY(BlueprintAssignable, Category = "UI|Toggle")
	FOnToggleGroupChanged OnSelectionChanged;

	void HandleToggleClicked(UToggleButtonWidget* Toggle);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Toggle", meta = (BindWidget))
	TObjectPtr<UNamedSlot> Slot_Toggles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Toggle")
	bool bAllowSwitchOff = false;

private:
	void ClearToggles();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UToggleButtonWidget>> Toggles;
};
