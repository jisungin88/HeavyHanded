#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToastWidget.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EHHToastType: uint8
{
	Info,
	Success,
	Warning,
	Error,
};

USTRUCT(BlueprintType)
struct FHHToast
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toast")
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toast")
	EHHToastType Type = EHHToastType::Info;

	/** 같은 key일 경우는 덮는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toast")
	FName Key;

	/** 보여지는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toast", meta = (ClampMin = "0"))
	float Duration = 0.f;

	/** 텍스트 앞에 보여지는 아이콘 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toast")
	TObjectPtr<UTexture2D> Icon;
};

UCLASS(Abstract)
class HEAVYHANDED_API UToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Toast")
	void OnToastShown(const FHHToast& Toast, bool bReplaced);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Toast")
	void OnToastHidden();

protected:
	virtual void NativeConstruct() override;
};
