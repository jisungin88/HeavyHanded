#pragma once

#include "CoreMinimal.h"
#include "UI/Shelter/ShelterWidgetBase.h"
#include "Core/PlayerStates/ShelterPlayerState.h"
#include "JobSelectWidget.generated.h"

class AShelterGameState;
class UButton;
class UEditableTextBox;

UCLASS(Abstract)
class HEAVYHANDED_API UJobSelectWidget : public UShelterWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI|Shelter")
	void RequestSelectJob(EJobType NewJob);

	UFUNCTION(BlueprintCallable, Category = "UI|Shelter")
	void RequestConfirmJob();

	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	bool IsJobTaken(EJobType Job) const;

	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	EJobType GetMySelectedJob() const;

	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	bool CanConfirm() const;

protected:
	virtual void TryBind() override;
	virtual void Unbind() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Input_Nickname;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shleter")
	void OnNicknameFeedback(const FText& Message, bool bOk);

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UButton> Btn_Confirm;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shelter")
	void OnJobStateRefreshed();

private:
	UFUNCTION()
	void HandleJobStateChanged();

	UFUNCTION()
	void HandleSelectedJobChanged(AShelterPlayerState* PlayerState);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleNicknameChanged(const FText& Text);

	UFUNCTION()
	void HandleNicknameRejected(ENicknameError Error);

	ENicknameError CheckNickname(FString& OutClean) const;

	void Refresh();

	UPROPERTY()
	TObjectPtr<AShelterGameState> BoundState;

	UPROPERTY()
	TObjectPtr<AShelterPlayerState> BoundPlayerState;
};
