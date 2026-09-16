#pragma once

#include "CoreMinimal.h"
#include "UI/Shelter/ShelterWidgetBase.h"
#include "RoomInfoWidget.generated.h"

class AShelterGameState;
class UTextBlock;

UCLASS(Abstract)
class HEAVYHANDED_API URoomInfoWidget : public UShelterWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void TryBind() override;
	virtual void Unbind() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RoomCode;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_PlayerCount;

private:
	UFUNCTION()
	void HandlePlayerCountChanged(int32 PlayerCount);

	void ApplyRoomText();

	void ApplyPlayerCount(int32 PlayerCount);

	UPROPERTY()
	TObjectPtr<AShelterGameState> BoundState;
};
