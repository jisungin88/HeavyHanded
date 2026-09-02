#include "UI/LoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"   // GetViewportSize — 지금 창 크기를 알아야 배율이 나온다
#include "Engine/UserInterfaceSettings.h"  // GetDPIScaleBasedOnSize — 뷰포트가 쓰는 것과 같은 곡선
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MoviePlayer.h"
#include "TimerManager.h"
#include "UI/HeavyUILog.h"
#include "UI/LoadingScreenWidget.h"
#include "UI/UISettings.h"
#include "Widgets/Layout/SDPIScaler.h"

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 서버든 클라든 각자의 GameInstance 에서 자기 화면을 띄운다.
	// 여기에 권위 게이트를 두면 안 된다 — 로딩 화면은 게임플레이 판정이 아니라
	// 각 플레이어가 자기 눈으로 봐야 하는 것이다
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ULoadingScreenSubsystem::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULoadingScreenSubsystem::HandlePostLoadMap);
}

void ULoadingScreenSubsystem::Deinitialize()
{
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Deinitialize();
}

void ULoadingScreenSubsystem::SetNextDestination(FGameplayTag SiteTag)
{
	DestinationSite = SiteTag;

	UE_LOG(LogHeavyUI, Log, TEXT("다음 로딩 화면의 목적지 — %s"),
		SiteTag.IsValid() ? *SiteTag.ToString() : TEXT("(없음)"));
}

FLoadingScreenContent ULoadingScreenSubsystem::MakeContent(FGameplayTag SiteTag, const FText& StatusOverride)
{
	const UUISettings* Settings = UUISettings::Get();

	FLoadingScreenContent Result;
	Result.Eyebrow = Settings->LoadingEyebrowText;
	Result.Title = FText::Format(Settings->LoadingTitleFormat, Settings->GetSiteDisplayName(SiteTag));
	Result.TipLabel = Settings->LoadingTipLabelText;
	Result.Tip = PickTip(SiteTag);
	Result.Status = StatusOverride.IsEmpty() ? Settings->LoadingStatusText : StatusOverride;

	return Result;
}

ULoadingScreenWidget* ULoadingScreenSubsystem::CreateScreenWidget()
{
	const UUISettings* Settings = UUISettings::Get();

	if (Settings->LoadingScreenWidgetClass.IsNull())
	{
		UE_LOG(LogHeavyUI, Warning,
			TEXT("로딩 화면 위젯 클래스가 비어 있습니다. Project Settings → Game → UI → Loading 에 WBP_Loading 을 지정할 것."));
		return nullptr;
	}

	UClass* WidgetClass = Settings->LoadingScreenWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("로딩 화면 위젯 클래스 '%s' 를 로드하지 못했습니다."),
			*Settings->LoadingScreenWidgetClass.ToString());
		return nullptr;
	}

	// 소유자를 GameInstance 로 둔다. 월드를 주면 트래블 도중 위젯이 같이 정리된다
	return CreateWidget<ULoadingScreenWidget>(GetGameInstance(), WidgetClass);
}

void ULoadingScreenSubsystem::HandlePreLoadMap(const FString& MapName)
{
	if (!IsMoviePlayerEnabled())
	{
		// PIE 에는 무비 플레이어가 없다. 실패가 아니라 이 환경의 정상 동작이다
		UE_LOG(LogHeavyUI, Log,
			TEXT("로딩 화면 생략 — 이 환경에는 무비 플레이어가 없습니다(PIE). "
				 "화면 확인은 hh.UI.Loading, 실제 동작은 Standalone 이나 패키징에서. 목적지 %s"), *MapName);
		return;
	}

	ULoadingScreenWidget* Widget = CreateScreenWidget();
	if (!Widget)
	{
		return;   // 이유는 CreateScreenWidget 이 이미 찍었다
	}

	Widget->ApplyContent(MakeContent(DestinationSite, FText::GetEmpty()));

	// 앞 판의 위젯은 여기서 교체한다. 로딩이 끝나는 시점에 놓아 버리면
	// 무비 플레이어가 아직 들고 있는 Slate 를 GC 가 먼저 회수할 수 있다
	ActiveWidget = Widget;

	const UUISettings* Settings = UUISettings::Get();

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;   // 맵 로드가 끝나면 알아서 내려간다
	Attributes.bWaitForManualStop = false;
	Attributes.bMoviesAreSkippable = false;
	Attributes.MinimumLoadingScreenDisplayTime = Settings->LoadingMinDisplaySeconds;
	// [왜 감싸는가] 무비 플레이어는 게임 뷰포트 바깥에서 그린다. 해상도에 맞춰 UI 를
	// 줄이고 늘리는 일은 뷰포트가 해 주던 것이라, 그냥 넘기면 배율이 항상 1 로 붙는다 —
	// 작은 창에서 44pt 제목이 화면을 덮는다. 뷰포트가 쓰는 것과 같은 곡선을 여기서 직접 건다.
	//
	// [왜 값이 아니라 람다인가] 로딩 중에 창 크기가 바뀔 수 있고, 이 위젯은 그때
	// 다시 만들어지지 않는다. 매번 읽어야 따라간다
	Attributes.WidgetLoadingScreen =
		SNew(SDPIScaler)
		.DPIScale(TAttribute<float>::CreateLambda([]()
			{
				// 뷰포트가 아직 없으면(창이 뜨기 전) 1080p 기준으로 둔다
				FVector2D ViewportSize(1920.f, 1080.f);
				if (GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->GetViewportSize(ViewportSize);
				}

				return GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(
					FIntPoint(FMath::RoundToInt(ViewportSize.X), FMath::RoundToInt(ViewportSize.Y)));
			}))
		[
			Widget->TakeWidget()
		];

	GetMoviePlayer()->SetupLoadingScreen(Attributes);

	UE_LOG(LogHeavyUI, Log, TEXT("로딩 화면 — 목적지 %s / 맵 %s"),
		DestinationSite.IsValid() ? *DestinationSite.ToString() : TEXT("(미지정)"), *MapName);
}

void ULoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 무비 플레이어는 bAutoCompleteWhenLoadingCompletes 로 스스로 내려간다.
	// 여기서 할 일은 다음 판의 팁이 직전과 겹치지 않게 두는 것뿐이다.
	//
	// ActiveWidget 은 일부러 비우지 않는다 (선언부 주석 참조)
	UE_LOG(LogHeavyUI, Verbose, TEXT("맵 로드 완료 — %s"), *GetNameSafe(LoadedWorld));
}

void ULoadingScreenSubsystem::EnsureTipsLoaded()
{
	if (bTipsResolved)
	{
		return;
	}

	// 실패해도 다시 시도하지 않는다. 이 함수는 트래블마다 불리고,
	// 표가 없는 상태에서 매번 로드를 시도하면 같은 경고가 계속 쌓인다
	bTipsResolved = true;

	const UUISettings* Settings = UUISettings::Get();
	if (Settings->LoadingTipsTable.IsNull())
	{
		UE_LOG(LogHeavyUI, Log,
			TEXT("로딩 팁 표가 지정되지 않았습니다. 팁 없이 제목만 표시합니다. "
				 "Project Settings → Game → UI → Loading 에서 DT_LoadingTips 를 지정할 것."));
		return;
	}

	TipsTable = Settings->LoadingTipsTable.LoadSynchronous();
	if (!TipsTable)
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("로딩 팁 표 '%s' 를 로드하지 못했습니다."),
			*Settings->LoadingTipsTable.ToString());
	}
}

FText ULoadingScreenSubsystem::PickTip(FGameplayTag SiteTag)
{
	EnsureTipsLoaded();

	if (!TipsTable)
	{
		return FText::GetEmpty();
	}

	TArray<FLoadingTipRow*> Rows;
	TipsTable->GetAllRows<FLoadingTipRow>(TEXT("ULoadingScreenSubsystem::PickTip"), Rows);

	// 이 장소의 팁 + 장소를 안 가리는 공용 팁을 함께 후보로 둔다.
	// 장소별 팁을 한 줄도 안 채운 상태에서도 화면이 비지 않게 하려는 것이다
	TArray<const FLoadingTipRow*> Candidates;
	for (const FLoadingTipRow* Row : Rows)
	{
		if (!Row || Row->TipText.IsEmpty())
		{
			continue;
		}

		const bool bMatchesSite = !Row->SiteTag.IsValid() || (SiteTag.IsValid() && Row->SiteTag.MatchesTag(SiteTag));
		if (bMatchesSite)
		{
			Candidates.Add(Row);
		}
	}

	if (Candidates.Num() == 0)
	{
		UE_LOG(LogHeavyUI, Log, TEXT("%s 에 맞는 로딩 팁이 없습니다. 팁 없이 표시합니다."),
			SiteTag.IsValid() ? *SiteTag.ToString() : TEXT("(장소 미지정)"));
		return FText::GetEmpty();
	}

	const FLoadingTipRow* Picked = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	// 두 번 연속 같은 팁만 피한다. 후보가 하나뿐이면 어쩔 수 없이 그대로 나간다
	if (Candidates.Num() > 1 && Picked->TipText.ToString() == LastTipKey)
	{
		Picked = Candidates[(Candidates.IndexOfByKey(Picked) + 1) % Candidates.Num()];
	}

	LastTipKey = Picked->TipText.ToString();
	return Picked->TipText;
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 치트
//
// 무비 플레이어는 PIE 에 없어서 로딩 화면을 에디터에서 볼 방법이 없다.
// 글자 크기 · 여백을 맞추려면 화면을 봐야 하므로 뷰포트에 직접 띄우는 길을 연다.
// 이 경로는 MoviePlayer 를 거치지 않는다 — 보이는 모양만 같다.
//
// DamageVignetteWidget.cpp 의 hh.UI.Damage 와 규칙을 맞춘다.
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

static void LoadingScreenPreviewCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	ULoadingScreenSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<ULoadingScreenSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("ULoadingScreenSubsystem 을 찾지 못했습니다."));
		return;
	}

	const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.f;

	// 인자로 장소를 주면 그 제목으로, 없으면 마지막 목적지로 그린다
	FGameplayTag SiteTag = Subsystem->GetDestinationSite();
	if (Args.Num() > 1)
	{
		SiteTag = FGameplayTag::RequestGameplayTag(FName(*Args[1]), /*ErrorIfNotFound=*/false);
		if (!SiteTag.IsValid())
		{
			UE_LOG(LogHeavyUI, Warning, TEXT("'%s' 는 등록된 태그가 아닙니다. 예: Site.Museum"), *Args[1]);
		}
	}

	ULoadingScreenWidget* Widget = Subsystem->CreateScreenWidget();
	if (!Widget)
	{
		return;   // 이유는 CreateScreenWidget 이 이미 찍었다
	}

	Widget->ApplyContent(Subsystem->MakeContent(SiteTag, FText::GetEmpty()));
	Widget->AddToViewport(1000);   // HUD 보다 위에 그린다

	// 화면을 가린 채로 남지 않도록 반드시 스스로 걷힌다
	FTimerHandle Handle;
	TWeakObjectPtr<ULoadingScreenWidget> WeakWidget(Widget);
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWidget]()
		{
			if (ULoadingScreenWidget* Live = WeakWidget.Get())
			{
				Live->RemoveFromParent();
			}
		}), FMath::Max(Seconds, 0.1f), /*bLoop=*/false);
}

static FAutoConsoleCommandWithWorldAndArgs GLoadingScreenPreviewCommand(
	  TEXT("hh.UI.Loading"),
	  TEXT("hh.UI.Loading <초> [Site 태그] — 로딩 화면을 뷰포트에 띄운다. 예: hh.UI.Loading 8 Site.Museum"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LoadingScreenPreviewCommand),
	  ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING
