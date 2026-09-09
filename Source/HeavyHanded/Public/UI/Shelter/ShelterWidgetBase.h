#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShelterWidgetBase.generated.h"

class AShelterGameState;
class AShelterPlayerController;
class AShelterPlayerState;


UCLASS(Abstract)
class HEAVYHANDED_API UShelterWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 구독 */
	virtual void TryBind() {}

	/** 구독 해제 */
	virtual void Unbind() {}

	void ScheduleRebind();

	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	AShelterGameState* GetShelterGameState() const;

	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	AShelterPlayerState* GetMyShelterPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	AShelterPlayerController* GetShelterPC() const;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Shelter", meta = (ClampMin = "0.05", Units = "s"))
	float BindRetryInterval = 0.25f;

private:
	FTimerHandle BindRetryHandle;
};
