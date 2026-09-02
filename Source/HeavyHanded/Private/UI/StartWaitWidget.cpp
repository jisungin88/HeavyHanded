#include "UI/StartWaitWidget.h"

#include "Components/BackgroundBlur.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UI/HeavyUILog.h"
#include "UI/UISettings.h"

void UStartWaitWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 이 위젯은 절대 클릭을 먹으면 안 된다. 입력 차단은 AHeistPlayerController 가 이미 했고,
	// 여기서 또 가로채면 대기가 풀린 뒤에도 조작이 먹지 않는 것처럼 보인다
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	ApplyTokens();

	// 디자이너에는 아직 게임에서 온 값이 없다. 미리보기로 채워야 채워진 바와
	// 빈 바의 색 차이를 눈으로 맞출 수 있다
	if (IsDesignTime())
	{
		Refresh(PreviewConnected, PreviewExpected);
	}
	else
	{
		Refresh(NumConnected, NumExpected);
	}
}

void UStartWaitWidget::UpdateWaitState(bool bInWaiting, int32 InNumConnected, int32 InNumExpected)
{
	bWaiting = bInWaiting;
	NumConnected = InNumConnected;
	NumExpected = InNumExpected;

	// 대기가 끝나면 스스로 접힌다. 띄운 쪽이 걷어 주기를 기다리지 않는 이유는,
	// 걷는 것을 한 군데라도 빠뜨리면 화면에 "4 / 4 함께하는 중" 이 남은 채로 판이 시작되기 때문이다
	SetVisibility(bWaiting ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	Refresh(NumConnected, NumExpected);
}

void UStartWaitWidget::ApplyTokens()
{
	const UUISettings* Settings = UUISettings::Get();

	if (Txt_Status)
	{
		Txt_Status->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Status->SetColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::TextPrimary));
	}

	if (Blur_Bg)
	{
		Blur_Bg->SetBlurStrength(Settings->PartyWaitBlurStrength);
	}

	if (Img_Dim)
	{
		// 배경색을 그대로 쓰되 투명도만 얹는다. 어둡게 덮는 판을 별도 색 토큰으로 두면
		// 배경색을 바꿨을 때 이것만 옛날 색으로 남는다
		FLinearColor Dim = UUISettings::GetUIColor(EUIColorToken::BgBase);
		Dim.A = Settings->PartyWaitDimOpacity;
		Img_Dim->SetColorAndOpacity(Dim);
	}
}

void UStartWaitWidget::Refresh(int32 Connected, int32 Expected)
{
	const UUISettings* Settings = UUISettings::Get();

	// 예정 인원 0 은 "아직 몇 명이 올지 모른다" 는 뜻이다 (코어 루프의 조용 시간 폴백 경로).
	// 이때 서식을 그대로 쓰면 "2 / 0 함께하는 중" 이 나간다
	const bool bKnownExpected = Expected > 0;

	if (Txt_Status)
	{
		Txt_Status->SetText(bKnownExpected
			? FText::Format(Settings->WaitingStatusFormat,
				FText::AsNumber(Connected), FText::AsNumber(Expected))
			: Settings->WaitingStatusUnknownText);
	}

	if (!Box_Slots)
	{
		return;
	}

	// 인원을 모르면 바를 몇 칸 그릴지도 모른다. 4칸을 놔두면 실제 인원과 어긋난 채로 보인다
	Box_Slots->SetVisibility(bKnownExpected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (!bKnownExpected)
	{
		return;
	}

	// 서버가 세는 값이라 정상 범위지만, 늦게 붙은 클라이언트가 중간 값을 받는 순간이 있다.
	// 여기서 한 번 가둬야 바가 칸 수보다 많이 채워지는 것을 막는다
	const int32 FilledCount = FMath::Clamp(Connected, 0, Expected);

	const FLinearColor FilledColor = UUISettings::GetUIColor(EUIColorToken::PartySlotFilled);
	const FLinearColor EmptyColor  = UUISettings::GetUIColor(EUIColorToken::PartySlotEmpty);

	const int32 SlotCount = Box_Slots->GetChildrenCount();

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		UWidget* SlotWidget = Box_Slots->GetChildAt(Index);
		if (!SlotWidget)
		{
			continue;
		}

		// 2~3인 판이면 남는 칸은 접는다. 회색으로 남겨 두면 오지 않을 사람을
		// 기다리는 것처럼 보인다
		if (Index >= Expected)
		{
			SlotWidget->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		SlotWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 색은 Image 에만 칠할 수 있다. 다른 타입을 넣어도 칸을 접는 것까지는 동작한다
		if (UImage* Bar = Cast<UImage>(SlotWidget))
		{
			Bar->SetColorAndOpacity(Index < FilledCount ? FilledColor : EmptyColor);
		}
	}
}

#if !UE_BUILD_SHIPPING

// 접속 대기는 넷이 서로 다른 속도로 들어와야 재현되는 구간이라 눈으로 맞추기가 어렵다.
// 이 명령은 서버 없이 화면만 띄운다 — 로딩 화면(hh.UI.Loading)과 달리 무비 플레이어를
// 타지 않는 뷰포트 위젯이라 PIE 에서도 그대로 보인다
static void PartyWaitPreviewCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
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
		UE_LOG(LogHeavyUI, Warning, TEXT("접속 대기 위젯 클래스 '%s' 를 로드하지 못했습니다."),
			*Settings->StartWaitWidgetClass.ToString());
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("접속 대기 미리보기 실패 — 플레이어 컨트롤러가 없습니다."));
		return;
	}

	const int32 Connected = (Args.Num() > 0) ? FCString::Atoi(*Args[0]) : 2;
	const int32 Expected  = (Args.Num() > 1) ? FCString::Atoi(*Args[1]) : 4;
	const float Seconds   = (Args.Num() > 2) ? FCString::Atof(*Args[2]) : 6.f;

	UStartWaitWidget* Widget = CreateWidget<UStartWaitWidget>(PC, WidgetClass);
	if (!Widget)
	{
		return;
	}

	Widget->UpdateWaitState(/*bInWaiting=*/true, Connected, Expected);
	Widget->AddToViewport(900);   // HUD 위, 로딩 화면 아래

	// 화면을 덮은 채로 남지 않도록 반드시 스스로 걷힌다
	FTimerHandle Handle;
	TWeakObjectPtr<UStartWaitWidget> WeakWidget(Widget);
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWidget]()
		{
			if (UStartWaitWidget* Live = WeakWidget.Get())
			{
				Live->RemoveFromParent();
			}
		}), FMath::Max(Seconds, 0.1f), /*bLoop=*/false);
}

static FAutoConsoleCommandWithWorldAndArgs GPartyWaitPreviewCommand(
	  TEXT("hh.UI.PartyWait"),
	  TEXT("hh.UI.PartyWait [접속] [예정] [초] — 접속 대기 오버레이를 띄운다. 예: hh.UI.PartyWait 2 4"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PartyWaitPreviewCommand),
	  ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING
