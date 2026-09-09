#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Templates/SubclassOf.h"
#include "ShelterHUD.generated.h"

class AShelterPlayerState;
class UJobSelectWidget;
class UShelterHUDWidget;

/**
 * 은신처 화면의 소유자.
 *
 * 은신처 HUD 는 항상 떠 있고, 역할을 확정하기 전에만 그 위에 선택 화면이 얹힌다.
 * 배타가 아니라 겹침이라 WidgetSwitcher 가 아니라 ZOrder 다.
 *
 * 입력 모드(모달 / 게임 / 채팅)를 바꾸는 곳은 여기 하나뿐이다.
 */
UCLASS()
class HEAVYHANDED_API AShelterHUD : public AHUD
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	UShelterHUDWidget* GetShelterHUDWidget() const { return HUDWidget; }

	/** 역할 선택 화면. 이미 확정했으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	UJobSelectWidget* GetJobSelectWidget() const { return JobSelectWidget; }

	/** 채팅 입력을 열거나 닫는다. Enter 입력이 부른다 */
	UFUNCTION(BlueprintCallable, Category = "UI|Shelter")
	void SetChatFocused(bool bFocused);

	UFUNCTION(BlueprintPure, Category = "UI|Shelter")
	bool IsChatFocused() const { return bChatFocused; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shelter")
	TSubclassOf<UShelterHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shelter")
	TSubclassOf<UJobSelectWidget> JobSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shelter")
	int32 HUDZOrder = 0;

	/** 은신처 HUD 위에 올라와야 한다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shelter")
	int32 JobSelectZOrder = 10;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Shelter", meta = (ClampMin = "0.05", Units = "s"))
	float BindRetryInterval = 0.25f;

private:
	/** PlayerState 는 늦게 온다. 붙을 때까지 재시도한다 */
	void TryBindPlayerState();

	UFUNCTION()
	void HandleJobConfirmedChanged(AShelterPlayerState* PlayerState);

	/** 확정 여부에 맞는 화면을 띄운다. 이 판단은 이 함수 하나에만 있다 */
	void ApplyJobConfirmed(bool bConfirmed);

	void ShowJobSelect();
	void HideJobSelect();

	void HandleChatDismissed();

	UPROPERTY()
	TObjectPtr<UShelterHUDWidget> HUDWidget;

	UPROPERTY()
	TObjectPtr<UJobSelectWidget> JobSelectWidget;

	UPROPERTY()
	TObjectPtr<AShelterPlayerState> BoundState;

	FTimerHandle BindRetryHandle;

	FDelegateHandle ChatDismissedHandle;

	bool bChatFocused = false;
};
