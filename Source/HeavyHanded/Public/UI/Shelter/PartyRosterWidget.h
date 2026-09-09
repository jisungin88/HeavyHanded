#pragma once

#include "CoreMinimal.h"
#include "UI/Shelter/ShelterWidgetBase.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "PartyRosterWidget.generated.h"

class AShelterGameState;
class UPanelWidget;

UCLASS(Abstract)
class HEAVYHANDED_API UPartyRosterWidget : public UShelterWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void TryBind() override;
	virtual void Unbind() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Slots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shelter")
	TSubclassOf<UUserWidget> SlotWidgetClass;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shelter")
	void OnRosterRefreshed(const TArray<AShelterPlayerState*>& Players);

private:
	UFUNCTION()
	void HandleJobStateChanged();

	UFUNCTION()
	void HandlePlayerCountChanged(int32 PlayerCount);

	void RefreshRoster();

	UPROPERTY()
	TObjectPtr<AShelterGameState> BoundState;
};
