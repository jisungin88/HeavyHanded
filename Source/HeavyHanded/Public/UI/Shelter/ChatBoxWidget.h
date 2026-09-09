#pragma once

#include "CoreMinimal.h"
#include "UI/Shelter/ShelterWidgetBase.h"
#include "Types/SlateEnums.h"
#include "ChatBoxWidget.generated.h"

class AShelterPlayerController;
class UEditableTextBox;
class UScrollBox;

DECLARE_MULTICAST_DELEGATE(FOnChatDismissed);

UCLASS(Abstract)
class HEAVYHANDED_API UChatBoxWidget : public UShelterWidgetBase
{
	GENERATED_BODY()

public:
	/** 입력창에 포커스를 준다. 입력 모드를 UI 로 바꾼 뒤에 부를 것 */
	void FocusInput();

	/** 입력창을 비운다. */
	void ClearInput();

	/** 입력창 온/오프 */
	void SetInputVisible(bool bVisible);

	/** 채팅창을 닫아야 한다. AShelterHUD 가 입력 모드를 되돌리려고 구독한다 */
	FOnChatDismissed OnChatDismissed;

protected:
	virtual void TryBind() override;
	virtual void Unbind() override;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UEditableTextBox> Input_Chat;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Shelter", meta = (BindWidget))
	TObjectPtr<UScrollBox> Scroll_Chat;

	/** BP에서 생성 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Shelter")
	void OnChatLineAdded(const FText& Line);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Shelter")
	int32 MaxChatLine = 30;

private:
	UFUNCTION()
	void HandleChatMessage(const FString& PlayerName, const FString& Message);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY()
	TObjectPtr<AShelterPlayerController> BoundPC;


};
