#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpectateOverlayWidget.generated.h"

class UHeistSpectatorComponent;
class UTextBlock;
class APlayerState;

UCLASS(Abstract)
class HEAVYHANDED_API USpectateOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 보고 있는 팀원 이름 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_ViewedName;

private:
	UFUNCTION()
	void HandleStateChanged(bool bSpectating);

	UFUNCTION()
	void HandleViewedChanged(APlayerState* NewViewed);

	void Bind();
	void Refresh(bool bSpectating, APlayerState* Viewed);

	UPROPERTY()
	TObjectPtr<UHeistSpectatorComponent> Bound;
};
