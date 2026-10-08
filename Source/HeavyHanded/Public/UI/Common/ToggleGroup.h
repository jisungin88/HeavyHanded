#pragma once

#include "CoreMinimal.h"
#include "Components/Overlay.h"
#include "ToggleGroup.generated.h"

class UToggleButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToggleGroupChanged, UToggleButton*, Selected, FName, SelectedId);

UCLASS()
class HEAVYHANDED_API UToggleGroup : public UOverlay
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Toggle")
	void RefreshToggles();

	UFUNCTION(BlueprintCallable, Category = "Toggle")
	void SelectToggle(UToggleButton* Toggle, bool bBroadcast = false);

	UFUNCTION(BlueprintCallable, Category = "Toggle")
	bool SelectById(FName Id, bool bBroadcast = false);

	UFUNCTION(BlueprintPure, Category = "Toggle")
	UToggleButton* GetSelectedToggle() const;

	UFUNCTION(BlueprintPure, Category = "Toggle")
	FName GetSelectedId() const;

	UFUNCTION(BlueprintPure, Category = "Toggle")
	TArray<UToggleButton*> GetToggles() const { return Toggles; }

	UPROPERTY(BlueprintAssignable, Category = "Toggle|Event")
	FOnToggleGroupChanged OnSelectionChanged;

	void HandleToggleClicked(UToggleButton* Toggle);

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual void OnWidgetRebuilt() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Toggle")
	bool bAllowSwitchOff = false;

private:
	void ClearToggles();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UToggleButton>> Toggles;
};
