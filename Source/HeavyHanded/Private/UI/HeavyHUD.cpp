#include "UI/HeavyHUD.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
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
	BindToHeistGameState();
}

void AHeavyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 구독을 안 풀면 HUD 가 사라진 뒤에도 델리게이트에 남는다
	if (AHeistGameState* GS = BoundState.Get())
	{
		GS->OnPhaseChanged.RemoveDynamic(this, &AHeavyHUD::HandlePhaseChanged);
	}
	BoundState = nullptr;

	if (GameStateSetHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GameStateSetEvent.Remove(GameStateSetHandle);
		}
		GameStateSetHandle.Reset();
	}

	// 레벨 이동 시 위젯이 화면에 남지 않게 직접 뗀다.
	// 뷰포트가 들고 있는 참조는 액터가 사라져도 자동으로 정리되지 않는다
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	RemoveResultWidget();

	Super::EndPlay(EndPlayReason);
}

// ──────────────────────────────────────────────────────────────
// 페이즈 구독
// ──────────────────────────────────────────────────────────────

void AHeavyHUD::BindToHeistGameState()
{
	if (BoundState.Get())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 호스트는 BeginPlay 시점에 GameState 가 이미 있고, 클라이언트는 대개 아직 없다.
	// 두 경로를 한 함수로 처리한다 — 어느 쪽이 먼저인지에 기대지 않는다
	if (AHeistGameState* GS = World->GetGameState<AHeistGameState>())
	{
		BoundState = GS;
		GS->OnPhaseChanged.AddDynamic(this, &AHeavyHUD::HandlePhaseChanged);

		// 구독 시점의 페이즈로 한 번 맞춘다. 안 하면 다음 전환까지 어긋난 화면이 남고,
		// 결과 페이즈에 늦게 들어온 사람은 결과 화면을 아예 못 본다
		ApplyPhaseLayer(GS->GetCurrentPhase());
		return;
	}

	// 아직 안 왔다. 도착하는 순간을 엔진이 알려준다
	if (!GameStateSetHandle.IsValid())
	{
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &AHeavyHUD::HandleGameStateSet);
	}
}

void AHeavyHUD::HandleGameStateSet(AGameStateBase* NewGameState)
{
	// 작업 레벨이 아니면 AHeistGameState 가 아닌 것이 온다. 그때는 아무 일도 하지 않는다 —
	// 페이즈가 없는 맵이므로 전환할 화면도 없고, HUD 는 그대로 떠 있으면 된다
	if (!Cast<AHeistGameState>(NewGameState))
	{
		return;
	}

	BindToHeistGameState();
}

void AHeavyHUD::HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag /*OldPhase*/, EHeistPhaseReason /*Reason*/)
{
	ApplyPhaseLayer(NewPhase);
}

void AHeavyHUD::ApplyPhaseLayer(FGameplayTag Phase)
{
	// [HUD 가시성은 여기서 건드리지 않는다]
	//   인게임 HUD 는 페이즈가 무엇이든 떠 있다 — 결과 화면에서도 마찬가지다.
	//   결과 화면은 HUD 를 덮는 것이 아니라 그 위에 얹히는 패널이다 (시안 ui_result.png).
	//   HUD 를 숨기는 경우는 컷신 하나뿐이고, 그건 페이즈가 아니라 시퀀스 재생이 정한다.
	//
	//   그래서 결과 페이즈에 미션 타이머 자리를 비우는 일은 UHeistHUDWidget 이 맡는다.
	//   결과 체류 시간(UHeistSettings::ResultSeconds)에도 카운트다운이 흐르기 때문에,
	//   걸러내지 않으면 판이 끝난 화면에서 미션 타이머가 되살아난다.

	// 부모 태그 매칭이다 — 나중에 Phase.Result 아래로 하위 페이즈가 생겨도 그대로 걸린다
	if (Phase.MatchesTag(HHTags::Phase_Result))
	{
		ShowResultWidget();
		return;
	}

	RemoveResultWidget();
}

// ──────────────────────────────────────────────────────────────
// 결과 화면
// ──────────────────────────────────────────────────────────────

void AHeavyHUD::ShowResultWidget()
{
	if (ResultWidget)
	{
		return;
	}

	if (!ResultWidgetClass)
	{
		if (!bWarnedNoResultClass)
		{
			bWarnedNoResultClass = true;

			// 결과 화면이 아직 없는 동안은 이것이 정상이라 Warning 이 아니다.
			// 다만 "결과 페이즈인데 화면이 비었다" 의 이유는 남겨 둔다
			UE_LOG(LogHeavyUI, Log,
				   TEXT("%s: ResultWidgetClass 가 비어 있어 결과 화면을 띄우지 않는다"),
				   *GetName());
		}
		return;
	}

	APlayerController* OwningPC = GetOwningPlayerController();
	if (!OwningPC || !OwningPC->IsLocalController())
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: 로컬 플레이어 컨트롤러가 없어 결과 화면을 만들지 않는다"), *GetName());
		return;
	}

	ResultWidget = CreateWidget<UUserWidget>(OwningPC, ResultWidgetClass);
	if (!ResultWidget)
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: 결과 화면 생성에 실패했다 (%s)"),
			   *GetName(), *GetNameSafe(ResultWidgetClass.Get()));
		return;
	}

	// HUD 와 같은 이유로 AddToPlayerScreen 이다 (CreateAndAddHUDWidget 주석 참고)
	if (!ResultWidget->AddToPlayerScreen(ResultZOrder))
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: 결과 화면을 플레이어 화면에 붙이지 못했다 (%s)"),
			   *GetName(), *GetNameSafe(ResultWidgetClass.Get()));
		ResultWidget = nullptr;
		return;
	}

	UE_LOG(LogHeavyUI, Log, TEXT("%s: 결과 화면 표시 (%s)"),
		   *GetName(), *GetNameSafe(ResultWidgetClass.Get()));
}

void AHeavyHUD::RemoveResultWidget()
{
	if (!ResultWidget)
	{
		return;
	}

	ResultWidget->RemoveFromParent();
	ResultWidget = nullptr;
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
