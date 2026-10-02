#include "UI/ToastComponent.h"

#include "Core/PlayerControllers/HeavyHandedPlayerController.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/HeavyUILog.h"
#include "UI/UISettings.h"

UToastComponent::UToastComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UToastComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}

	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}

	Queue.Reset();
	bShowing = false;

	Super::EndPlay(EndPlayReason);
}

void UToastComponent::ShowToast(const FHHToast& Toast)
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController() || Toast.Message.IsEmpty())
	{
		return;
	}

	if (!Toast.Key.IsNone())
	{
		if (IsBusy() && Current.Key == Toast.Key)
		{
			Show(Toast, true);
			return;
		}

		if (FHHToast* Queued = Queue.FindByPredicate([&Toast](const FHHToast& Q) { return Q.Key == Toast.Key; }))
		{
			*Queued = Toast;
			return;
		}
	}

	if (!IsBusy())
	{
		Show(Toast, false);
		return;
	}

	Queue.Add(Toast);

	const int32 MaxQueue = UUISettings::Get()->ToastMaxQueue;
	while (Queue.Num() > MaxQueue)
	{
		Queue.RemoveAt(0);
	}
}

void UToastComponent::ShowToastMessage(FText Message, EHHToastType Type, FName Key)
{
	FHHToast Toast;
	Toast.Message = Message;
	Toast.Type = Type;
	Toast.Key = Key;
	ShowToast(Toast);
}

void UToastComponent::ShowLocalToast(const UObject* WorldContextObject, const FHHToast& Toast)
{
	if (UToastComponent* Local = FindLocal(WorldContextObject))
	{
		Local->ShowToast(Toast);
	}
}

UToastComponent* UToastComponent::FindLocal(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
			: nullptr;

	const AHeavyHandedPlayerController* PC = World
			? Cast<AHeavyHandedPlayerController>(GEngine->GetFirstLocalPlayerController(World))
			: nullptr;

	return PC ? PC->GetToastComponent() : nullptr;
}

void UToastComponent::Show(const FHHToast& Toast, bool bReplaced)
{
	UToastWidget* ToastWidget = EnsureWidget();
	if (!ToastWidget)
	{
		return;
	}

	Current = Toast;
	bShowing = true;

	const float Duration = Toast.Duration > 0.f ? Toast.Duration : UUISettings::Get()->ToastDefaultDuration;
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &UToastComponent::HandleExpired, Duration, false);
	ToastWidget->OnToastShown(Toast, bReplaced);
}

void UToastComponent::HandleExpired()
{
	bShowing = false;

	if (Widget)
	{
		Widget->OnToastHidden();
	}

	GetWorld()->GetTimerManager().SetTimer(Timer, this, &UToastComponent::ShowNext, UUISettings::Get()->ToastGapSeconds, false);
}

void UToastComponent::ShowNext()
{
	if (Queue.IsEmpty())
	{
		return;
	}

	const FHHToast Next = Queue[0];
	Queue.RemoveAt(0);
	Show(Next, false);
}

UToastWidget* UToastComponent::EnsureWidget()
{
	if (Widget)
	{
		return Widget;
	}

	const UUISettings* Settings = UUISettings::Get();
	UClass* WidgetClass = Settings->ToastWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogHeavyUI, Warning,
		       TEXT("ToastWidgetClass 가 비어 있어 토스트를 띄우지 못했습니다. "
			       "Project Settings > Game > UI > Toast 에 WBP_Toast 를 지정하세요."));
		return nullptr;
	}

	Widget = CreateWidget<UToastWidget>(Cast<APlayerController>(GetOwner()), WidgetClass);
	if (Widget)
	{
		Widget->AddToViewport(Settings->ToastZOrder);
	}
	return Widget;
}

bool UToastComponent::IsBusy() const
{
	const UWorld* World = GetWorld();
	return bShowing || (World && World->GetTimerManager().IsTimerActive(Timer));
}

static void ToastCommand(const TArray<FString>& Args, UWorld* World)
{
	FHHToast Toast;
	Toast.Type = Args.IsValidIndex(0) ? static_cast<EHHToastType>(FMath::Clamp(FCString::Atoi(*Args[0]), 0, 3)) : EHHToastType::Info;
	Toast.Key = Args.IsValidIndex(1) && Args[1] != TEXT("-") ? FName(*Args[1]) : NAME_None;
	Toast.Message = FText::FromString(Args.Num() > 2 ? FString::Join(TArrayView<const FString>(Args).RightChop(2), TEXT(" ")) : TEXT("토스트 테스트"));
	UToastComponent::ShowLocalToast(World, Toast);
}

static FAutoConsoleCommandWithWorldAndArgs GToastCommand(
	TEXT("hh.Toast"),
	TEXT("hh.Toast [타입 0~3] [Key 또는 -] [내용] - 로컬 토스트를 띄운다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ToastCommand),
	ECVF_Cheat);
