#pragma once

#include "CoreMinimal.h"
#include "UI/Shelter/ShelterWidgetBase.h"
#include "ShelterHUDWidget.generated.h"

class UChatBoxWidget;
class UPartyActionsWidget;
class UPartyRosterWidget;
class URoomInfoWidget;

UCLASS(Abstract)
class HEAVYHANDED_API UShelterHUDWidget : public UShelterWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	UChatBoxWidget* GetChatBox() const { return W_Chat; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<URoomInfoWidget> W_RoomInfo;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UPartyRosterWidget> W_Roster;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UChatBoxWidget> W_Chat;

	// UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	// TObjectPtr<UPartyActionsWidget> W_Actions;
};
