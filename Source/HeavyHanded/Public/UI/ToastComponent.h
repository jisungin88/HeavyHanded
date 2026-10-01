#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/ToastWidget.h"
#include "ToastComponent.generated.h"


UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class HEAVYHANDED_API UToastComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToastComponent();

	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Toast")
	void ShowToast(const FHHToast& Toast);

	/** 문구만 띄울 때 */
	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Toast", meta = (AdvancedDisplay = "Key"))
	void ShowToastMessage(FText Message, EHHToastType Type = EHHToastType::Info, FName Key = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Toast", meta = (WorldContext = "WorldContextObject"))
	static void ShowLocalToast(const UObject* WorldContextObject, const FHHToast& Toast);

	static UToastComponent* FindLocal(const UObject* WorldContextObject);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Show(const FHHToast& Toast, bool bReplaced);
	void HandleExpired();
	void ShowNext();
	UToastWidget* EnsureWidget();

	bool IsBusy() const;

	UPROPERTY(Transient)
	TObjectPtr<UToastWidget> Widget;

	UPROPERTY(Transient)
	TArray<FHHToast> Queue;

	UPROPERTY(Transient)
	FHHToast Current;

	bool bShowing = false;
	FTimerHandle Timer;
};
