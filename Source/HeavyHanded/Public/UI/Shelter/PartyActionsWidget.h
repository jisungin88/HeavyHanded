#pragma once

#include "CoreMinimal.h"
#include "UI/Shelter/ShelterWidgetBase.h"
#include "PartyActionsWidget.generated.h"

class AShelterGameState;
class AShelterPlayerController;
class UButton;

UCLASS(Abstract)
class HEAVYHANDED_API UPartyActionsWidget : public UShelterWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void TryBind() override;
	virtual void Unbind() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UButton> Btn_Start;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UButton> Btn_Leave;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Shelter")
	FName LeaveMapName = TEXT("L_LoginTitle");

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shelter")
	void OnCanStartUpdate(bool bCanStart);

private:
	UFUNCTION()
	void HandleCanStartChanged(bool bCanStart);

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	void ApplyCanStart(bool bCanStart);

	UPROPERTY()
	TObjectPtr<AShelterGameState> BoundState;
};
