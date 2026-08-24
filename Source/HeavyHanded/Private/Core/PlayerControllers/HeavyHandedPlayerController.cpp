#include "Core/PlayerControllers/HeavyHandedPlayerController.h"

#include "Blueprint/UserWidget.h"

/**
 * 이 베이스만의 로그 카테고리.
 *
 * LogHeist 를 쓰지 않는 이유는 이 클래스가 코어 루프 소유가 아니기 때문이다 —
 * 은신처(세션 파트)와 작업 레벨이 같이 쓴다. 남의 카테고리에 로그를 섞으면
 * "이 판이 왜 Escape 로 넘어갔나" 를 따라가는 필터에 은신처 상점 로그가 끼어든다.
 * 문서 07 의 기본값(.cpp 안의 STATIC)이기도 하다.
 */
DEFINE_LOG_CATEGORY_STATIC(LogHHPlayerController, Log, All);

// ──────────────────────────────────────────────────────────────
// HUD
// ──────────────────────────────────────────────────────────────

UUserWidget* AHeavyHandedPlayerController::EnsureHUDWidget()
{
	// 리슨 서버에서는 호스트가 원격 클라이언트의 PlayerController 복제본도 들고 있다.
	// 거기서 위젯을 만들면 호스트 화면에 남의 HUD 가 하나 더 겹쳐 뜬다 —
	// 호스트에서만 화면이 이상한, 원인을 짚기 어려운 종류의 증상이 된다
	if (!IsLocalController())
	{
		return nullptr;
	}

	if (HUDWidget)
	{
		return HUDWidget;
	}

	if (!HUDWidgetClass)
	{
		UE_LOG(LogHHPlayerController, Warning,
			TEXT("%s 에 HUDWidgetClass 가 비어 있습니다. BP 에서 값을 지정해야 HUD 가 뜹니다."),
			*GetClass()->GetName());
		return nullptr;
	}

	HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
	if (!HUDWidget)
	{
		UE_LOG(LogHHPlayerController, Warning,
			TEXT("HUD 위젯 생성에 실패했습니다 (클래스 %s)."), *HUDWidgetClass->GetName());
		return nullptr;
	}

	HUDWidget->AddToViewport();
	return HUDWidget;
}

void AHeavyHandedPlayerController::RemoveHUDWidget()
{
	if (!HUDWidget)
	{
		return;
	}

	HUDWidget->RemoveFromParent();
	HUDWidget = nullptr;
}

// ──────────────────────────────────────────────────────────────
// 입력 포커스
// ──────────────────────────────────────────────────────────────

void AHeavyHandedPlayerController::EnterUIFocus(UObject* Requester, EHHUIFocusMode Mode)
{
	// 소유자 없는 포커스는 풀 사람이 없다는 뜻이다. 그대로 두면 커서가 영영 남는다
	if (!Requester)
	{
		UE_LOG(LogHHPlayerController, Warning,
			TEXT("EnterUIFocus 에 소유자가 없습니다. 무시합니다 — 푸는 쪽을 특정할 수 없습니다."));
		return;
	}

	if (bUIFocused && UIFocusOwner.IsValid() && UIFocusOwner.Get() != Requester)
	{
		UE_LOG(LogHHPlayerController, Warning,
			TEXT("UI 포커스를 %s 가 쥔 채로 %s 가 다시 요청했습니다. 넘겨받습니다 — "
			     "먼저 쥔 쪽이 ExitUIFocus 를 빠뜨렸을 수 있습니다."),
			*GetNameSafe(UIFocusOwner.Get()), *GetNameSafe(Requester));
	}

	UIFocusOwner = Requester;
	bUIFocused = true;
	ApplyInputMode(true, Mode);
}

void AHeavyHandedPlayerController::ExitUIFocus(UObject* Requester)
{
	if (!bUIFocused)
	{
		return;
	}

	// 쥔 객체가 아직 살아 있는데 남이 풀려고 하면 무시한다. 이 한 줄이 이 API 의 존재 이유다.
	// 이미 파괴된 경우(약참조가 죽은 경우)에는 누구든 치울 수 있게 둔다 — 안 그러면 영영 안 풀린다
	if (UIFocusOwner.IsValid() && UIFocusOwner.Get() != Requester)
	{
		UE_LOG(LogHHPlayerController, Verbose,
			TEXT("%s 가 UI 포커스를 풀려 했지만 쥔 쪽은 %s 입니다. 무시합니다."),
			*GetNameSafe(Requester), *GetNameSafe(UIFocusOwner.Get()));
		return;
	}

	UIFocusOwner.Reset();
	bUIFocused = false;
	ApplyInputMode(false, EHHUIFocusMode::GameAndUI);
}

void AHeavyHandedPlayerController::ApplyInputMode(bool bInUIFocused, EHHUIFocusMode Mode)
{
	// 입력 모드와 커서는 로컬 플레이어의 뷰포트 상태다. 원격 PC 복제본에서 만지면
	// 아무 일도 일어나지 않거나 호스트 화면이 바뀐다
	if (!IsLocalController())
	{
		return;
	}

	if (!bInUIFocused)
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		return;
	}

	if (Mode == EHHUIFocusMode::UIOnly)
	{
		SetInputMode(FInputModeUIOnly());
	}
	else
	{
		SetInputMode(FInputModeGameAndUI());
	}

	bShowMouseCursor = true;
}

// ──────────────────────────────────────────────────────────────

void AHeavyHandedPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveHUDWidget();

	Super::EndPlay(EndPlayReason);
}
