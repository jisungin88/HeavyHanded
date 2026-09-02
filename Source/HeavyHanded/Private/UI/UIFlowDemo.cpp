#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UI/HeavyUILog.h"
#include "UI/LoadingScreenSubsystem.h"
#include "UI/LoadingScreenWidget.h"
#include "UI/StartWaitWidget.h"
#include "UI/UISettings.h"

/**
 * 화면 흐름 데모 — 로딩 화면 → 접속 대기 오버레이 → 걷힘.
 *
 * [왜 필요한가] 진짜 흐름은 넷이 서로 다른 속도로 접속해야 나오는데, 그걸 재현하려면
 *   패키징하고 사람을 모아야 한다. 화면을 다듬는 동안 그걸 매번 할 수는 없다.
 *
 * [진짜가 아니라 흉내다] 여기서 띄우는 로딩 화면은 무비 플레이어를 타지 않는 그냥 위젯이다.
 *   그래서 **PIE 에서도 보인다** — 진짜 로딩 화면은 PIE 에 무비 플레이어가 없어서 안 뜬다.
 *   화면 순서와 문구를 눈으로 맞추는 용도이지, 로딩 동작을 검증하는 것이 아니다.
 *
 * [게임 상태를 건드리지 않는다] 페이즈도 인원도 바꾸지 않고 위젯만 띄웠다 내린다.
 *   그래서 아무 맵에서나 안전하게 돌릴 수 있다
 */
namespace
{
	/** 데모가 진행되는 동안만 사는 상태. 타이머 람다가 공유 참조로 붙들고 있는다 */
	struct FUIFlowDemoState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<ULoadingScreenWidget> LoadingWidget;
		TWeakObjectPtr<UStartWaitWidget> WaitWidget;

		/** 지금 몇 명 들어온 것으로 보여주고 있는가 */
		int32 Connected = 0;

		/** 몇 명까지 세고 끝낼 것인가 */
		int32 Expected = 4;

		/** 한 명씩 늘어나는 간격 */
		float Interval = 1.5f;

		FTimerHandle Handle;
	};

	void DemoFinish(TSharedRef<FUIFlowDemoState> State);
	void DemoAdvance(TSharedRef<FUIFlowDemoState> State);

	/** 다음 단계를 예약한다. 월드가 사라졌으면 조용히 멈춘다 */
	void DemoSchedule(TSharedRef<FUIFlowDemoState> State, float Delay, TFunction<void(TSharedRef<FUIFlowDemoState>)> Step)
	{
		UWorld* World = State->World.Get();
		if (!World)
		{
			return;
		}

		World->GetTimerManager().SetTimer(State->Handle, FTimerDelegate::CreateLambda([State, Step]()
			{
				Step(State);
			}), FMath::Max(Delay, 0.05f), /*bLoop=*/false);
	}

	/** 인원을 한 명 늘린다. 다 채웠으면 끝낸다 */
	void DemoAdvance(TSharedRef<FUIFlowDemoState> State)
	{
		UStartWaitWidget* Widget = State->WaitWidget.Get();
		if (!Widget)
		{
			return;
		}

		++State->Connected;

		if (State->Connected > State->Expected)
		{
			DemoFinish(State);
			return;
		}

		Widget->UpdateWaitState(/*bInWaiting=*/true, State->Connected, State->Expected);

		DemoSchedule(State, State->Interval, &DemoAdvance);
	}

	/** 전원이 모인 뒤 걷어낸다. 실제 게임에서도 여기서 Prep 45초가 시작된다 */
	void DemoFinish(TSharedRef<FUIFlowDemoState> State)
	{
		if (UStartWaitWidget* Widget = State->WaitWidget.Get())
		{
			// 위젯은 이걸 받으면 스스로 접힌다. 실제 흐름과 같은 경로를 태워 봐야
			// "다 모였는데 화면이 안 사라지더라" 를 여기서 잡을 수 있다
			Widget->UpdateWaitState(/*bInWaiting=*/false, State->Expected, State->Expected);
			Widget->RemoveFromParent();
		}

		UE_LOG(LogHeavyUI, Log, TEXT("화면 흐름 데모 끝."));
	}

	/** 로딩 화면을 걷고 접속 대기 오버레이로 넘어간다 */
	void DemoToStartWait(TSharedRef<FUIFlowDemoState> State)
	{
		if (ULoadingScreenWidget* Loading = State->LoadingWidget.Get())
		{
			Loading->RemoveFromParent();
		}

		UWorld* World = State->World.Get();
		if (!World)
		{
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			return;
		}

		const UUISettings* Settings = UUISettings::Get();
		if (Settings->StartWaitWidgetClass.IsNull())
		{
			UE_LOG(LogHeavyUI, Warning,
				TEXT("접속 대기 위젯 클래스가 비어 있습니다. Project Settings → Game → UI → Start Wait 에 WBP_StartWait 을 지정할 것."));
			return;
		}

		UClass* WidgetClass = Settings->StartWaitWidgetClass.LoadSynchronous();
		if (!WidgetClass)
		{
			return;
		}

		UStartWaitWidget* Widget = CreateWidget<UStartWaitWidget>(PC, WidgetClass);
		if (!Widget)
		{
			return;
		}

		State->WaitWidget = Widget;

		// 첫 명은 자기 자신이다 — 이 화면을 보고 있다는 것은 이미 들어와 있다는 뜻이라
		// 0/4 로 시작하는 화면은 실제로는 존재하지 않는다
		State->Connected = 1;
		Widget->UpdateWaitState(/*bInWaiting=*/true, State->Connected, State->Expected);
		Widget->AddToViewport(900);

		DemoSchedule(State, State->Interval, &DemoAdvance);
	}

	void UIFlowDemoCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		if (!GameInstance)
		{
			return;
		}

		ULoadingScreenSubsystem* Loading = GameInstance->GetSubsystem<ULoadingScreenSubsystem>();
		if (!Loading)
		{
			return;
		}

		const float LoadSeconds = (Args.Num() > 0) ? FCString::Atof(*Args[0]) : 3.f;
		const float Interval    = (Args.Num() > 1) ? FCString::Atof(*Args[1]) : 1.5f;

		// 장소를 안 주면 서브시스템이 기억하고 있는 목적지를 쓴다.
		// 그것도 없으면 제목이 "작업 장소 진입 중…" 으로 나간다 — 실패가 아니다
		FGameplayTag SiteTag = Loading->GetDestinationSite();
		if (Args.Num() > 2)
		{
			const FGameplayTag Parsed = FGameplayTag::RequestGameplayTag(FName(*Args[2]), /*ErrorIfNotFound=*/false);
			if (Parsed.IsValid())
			{
				SiteTag = Parsed;
			}
			else
			{
				UE_LOG(LogHeavyUI, Warning, TEXT("'%s' 는 등록된 태그가 아닙니다. 예: Site.Museum"), *Args[2]);
			}
		}

		ULoadingScreenWidget* LoadingWidget = Loading->CreateScreenWidget();
		if (!LoadingWidget)
		{
			return;   // 이유는 CreateScreenWidget 이 이미 찍었다
		}

		LoadingWidget->ApplyContent(Loading->MakeContent(SiteTag, FText::GetEmpty()));
		LoadingWidget->AddToViewport(1000);   // 접속 대기 오버레이보다 위

		TSharedRef<FUIFlowDemoState> State = MakeShared<FUIFlowDemoState>();
		State->World = World;
		State->LoadingWidget = LoadingWidget;
		State->Interval = FMath::Max(Interval, 0.05f);

		UE_LOG(LogHeavyUI, Log, TEXT("화면 흐름 데모 시작 — 로딩 %.1f초 → 접속 대기 %d명까지 %.1f초 간격"),
			LoadSeconds, State->Expected, State->Interval);

		DemoSchedule(State, LoadSeconds, &DemoToStartWait);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GUIFlowDemoCommand(
	  TEXT("hh.UI.Demo"),
	  TEXT("hh.UI.Demo [로딩초] [간격초] [Site 태그] — 로딩 화면 → 접속 대기 → 걷힘 순서를 한 번에 보여준다. 예: hh.UI.Demo 3 1.5 Site.Museum"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UIFlowDemoCommand),
	  ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING
