#pragma once

#include "CoreMinimal.h"
#include "UI/Shelter/ShelterWidgetBase.h"
#include "ShelterHUDWidget.generated.h"

class UChatBoxWidget;
class UPartyActionsWidget;
class UPartyRosterWidget;
class URoomInfoWidget;
class AShelterPlayerState;

UCLASS(Abstract)
class HEAVYHANDED_API UShelterHUDWidget : public UShelterWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	UChatBoxWidget* GetChatBox() const { return W_Chat; }

protected:
	virtual void TryBind() override;
	virtual void Unbind() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<URoomInfoWidget> W_RoomInfo;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UPartyRosterWidget> W_Roster;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UChatBoxWidget> W_Chat;

	// UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	// TObjectPtr<UPartyActionsWidget> W_Actions;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shelter")
	void BP_ShowArrestNotice(int32 RescueCost);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shelter")
	void BP_ShowRescuedNotice();

private:
	UFUNCTION()
	void HandleMyArrestedChanged(AShelterPlayerState* PlayerState);

	TWeakObjectPtr<AShelterPlayerState> BoundPlayerState;

	bool bArrestNoticeShown = false;
};
