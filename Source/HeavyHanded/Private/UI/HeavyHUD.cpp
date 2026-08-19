#include "UI/HeavyHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

#include "UI/HeavyUILog.h"

AHeavyHUD::AHeavyHUD()
{
	// 이 HUD 는 캔버스에 직접 그리지 않는다 (그건 DrawHUD/PostRender 경로이고 틱과 무관하다).
	// 위젯이 알아서 갱신되므로 액터 틱이 필요 없다
	PrimaryActorTick.bCanEverTick = false;
}

void AHeavyHUD::BeginPlay()
{
	Super::BeginPlay();

	CreateAndAddHUDWidget();
}

void AHeavyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 이동 시 위젯이 화면에 남지 않게 직접 뗀다.
	// 뷰포트가 들고 있는 참조는 액터가 사라져도 자동으로 정리되지 않는다
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AHeavyHUD::CreateAndAddHUDWidget()
{
	if (HUDWidget)
	{
		return;
	}

	if (!HUDWidgetClass)
	{
		// 여기서 조용히 돌아가면 "HUD 가 원래 없는 건가?" 로만 보인다.
		// BP 서브클래스에서 WBP 를 안 꽂은 것이 거의 항상 원인이다
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: HUDWidgetClass 가 비어 있어 HUD 를 만들지 않는다. "
					"BP 서브클래스에서 WBP_HUD 를 지정할 것"),
			   *GetName());
		return;
	}

	// AHUD 는 로컬 플레이어가 있는 컨트롤러에만 스폰되지만, 레벨 전환 도중처럼
	// 소유자가 아직/이미 없는 순간이 있다. 그때 만들면 위젯이 주인 없이 뜬다
	APlayerController* OwningPC = GetOwningPlayerController();
	if (!OwningPC || !OwningPC->IsLocalController())
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: 로컬 플레이어 컨트롤러가 없어 HUD 를 만들지 않는다"), *GetName());
		return;
	}

	HUDWidget = CreateWidget<UUserWidget>(OwningPC, HUDWidgetClass);
	if (!HUDWidget)
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: HUD 위젯 생성에 실패했다 (%s)"),
			   *GetName(), *GetNameSafe(HUDWidgetClass.Get()));
		return;
	}

	// AddToViewport 가 아니라 AddToPlayerScreen 을 쓴다.
	// 전자는 화면 전체에 붙어 로컬 멀티(분할 화면)에서 두 플레이어의 HUD 가 겹친다.
	// 후자는 이 컨트롤러의 화면에만 붙는다 — HUD 는 원래 플레이어별 화면이다
	if (!HUDWidget->AddToPlayerScreen(HUDZOrder))
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: HUD 위젯을 플레이어 화면에 붙이지 못했다 (%s)"),
			   *GetName(), *GetNameSafe(HUDWidgetClass.Get()));
		HUDWidget = nullptr;
		return;
	}

	UE_LOG(LogHeavyUI, Log, TEXT("%s: HUD 표시 (%s)"), *GetName(), *GetNameSafe(HUDWidgetClass.Get()));
}

void AHeavyHUD::SetHUDVisible(bool bVisible)
{
	if (!HUDWidget)
	{
		return;
	}

	// Hidden 이 아니라 Collapsed 다. 숨긴 위젯이 레이아웃 자리를 차지할 이유가 없다
	HUDWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible
									  : ESlateVisibility::Collapsed);
}

bool AHeavyHUD::IsHUDVisible() const
{
	return HUDWidget && HUDWidget->GetVisibility() != ESlateVisibility::Collapsed;
}
