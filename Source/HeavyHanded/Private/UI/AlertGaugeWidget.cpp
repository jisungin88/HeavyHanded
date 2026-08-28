#include "UI/AlertGaugeWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Alert/AlertComponent.h"
#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "UI/HeavyUILog.h"
#include "UI/UISettings.h"

void UAlertGaugeWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// PreConstruct 는 에디터 디자이너에서도 실행된다. 토큰을 여기서 적용해야
	// 편집 중에도 실제 색 · 폰트가 보이고, 디자이너에 직접 찍은 색이 에셋에 구워지는 것을 막는다.
	// UMG 에 구운 색은 UUISettings 를 고쳐도 따라오지 않는다 (UISettings.h 주석)
	if (Txt_Level)
	{
		Txt_Level->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
	}

	if (Txt_Percent)
	{
		Txt_Percent->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Percent->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextSecondary)));
	}

	// 디자이너에서도 빈 게이지가 아니라 현재 단계의 모습이 보이게 한 번 칠한다.
	// 아직 컴포넌트에 붙기 전이라 평온 · 0% 다
	ApplyLevelVisual(GetAlertLevel());

	LastShownPercent = INDEX_NONE;
	ApplyGaugeVisual(DisplayedGauge);
}

void UAlertGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
	BindToHeistGameState();
}

void UAlertGaugeWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
		World->GetTimerManager().ClearTimer(InterpHandle);
	}

	// 구독을 안 풀면 위젯이 사라진 뒤에도 델리게이트에 남는다
	if (UAlertComponent* Alert = BoundAlert.Get())
	{
		Alert->OnAlertGaugeChanged.RemoveDynamic(this, &UAlertGaugeWidget::HandleGaugeChanged);
		Alert->OnAlertLevelChanged.RemoveDynamic(this, &UAlertGaugeWidget::HandleLevelChanged);
	}
	BoundAlert = nullptr;

	if (AHeistGameState* GS = BoundState.Get())
	{
		GS->OnPhaseChanged.RemoveDynamic(this, &UAlertGaugeWidget::HandlePhaseChanged);
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

	Super::NativeDestruct();
}

void UAlertGaugeWidget::TryBind()
{
	if (BoundAlert.Get())
	{
		return;
	}

	UAlertComponent* Alert = UAlertComponent::Get(this);
	if (!Alert)
	{
		// 클라이언트에서는 GameState 와 경계도 컴포넌트가 복제로 뒤늦게 도착한다
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
					BindRetryHandle, this, &UAlertGaugeWidget::TryBind, BindRetryInterval, false);
		}
		return;
	}

	BoundAlert = Alert;

	Alert->OnAlertGaugeChanged.AddDynamic(this, &UAlertGaugeWidget::HandleGaugeChanged);
	Alert->OnAlertLevelChanged.AddDynamic(this, &UAlertGaugeWidget::HandleLevelChanged);

	// 구독 시점의 값으로 한 번 그린다. 안 하면 다음 소음이 날 때까지 빈 게이지가 보인다.
	//
	// 이때만은 보간하지 않고 즉시 맞춘다 — 보간하면 접속 직후 게이지가 0 에서 스르륵
	// 차올라 "지금 막 소음이 났다" 처럼 보인다. 실제로는 이미 그만큼 차 있던 것이다
	TargetGauge = DisplayedGauge = FMath::Clamp(Alert->GetAlertGauge01(), 0.f, 1.f);
	LastShownPercent = INDEX_NONE;
	ApplyGaugeVisual(DisplayedGauge);
	OnGaugeUpdated(TargetGauge);

	const EAlertLevel Level = Alert->GetAlertLevel();
	HandleLevelChanged(Level, Level);
}

// ──────────────────────────────────────────────────────────────
// 페이즈 — 언제 게이지를 보여줄 것인가
// ──────────────────────────────────────────────────────────────

void UAlertGaugeWidget::BindToHeistGameState()
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

	if (AHeistGameState* GS = World->GetGameState<AHeistGameState>())
	{
		BoundState = GS;
		GS->OnPhaseChanged.AddDynamic(this, &UAlertGaugeWidget::HandlePhaseChanged);

		// 구독 시점의 페이즈로 한 번 맞춘다. 안 하면 다음 전환까지 어긋난 화면이 남는다
		ApplyPhaseVisibility(GS->GetCurrentPhase());
		return;
	}

	// 아직 안 왔다. 도착하는 순간을 엔진이 알려준다.
	// 끝내 안 오면(작업 레벨이 아니면) 아무 일도 일어나지 않고 게이지는 계속 보인다
	if (!GameStateSetHandle.IsValid())
	{
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &UAlertGaugeWidget::HandleGameStateSet);
	}
}

void UAlertGaugeWidget::HandleGameStateSet(AGameStateBase* NewGameState)
{
	if (!Cast<AHeistGameState>(NewGameState))
	{
		return;   // 페이즈가 없는 맵이다. 게이지는 그대로 보이면 된다
	}

	BindToHeistGameState();
}

void UAlertGaugeWidget::HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag /*OldPhase*/, EHeistPhaseReason /*Reason*/)
{
	ApplyPhaseVisibility(NewPhase);
}

void UAlertGaugeWidget::ApplyPhaseVisibility(FGameplayTag Phase)
{
	// [화이트리스트인 이유] "숨길 페이즈" 를 나열하면 페이즈가 새로 생길 때마다
	//   아무도 손대지 않아도 게이지가 저절로 그 화면에 나타난다. 컷신 페이즈가
	//   추가되는 것이 예정돼 있어서 더 그렇다 — 기본값은 "안 보임" 이어야 한다.
	//
	// 준비  — 이때의 경계도는 본 작업 진입에서 ResetAlert() 로 지워진다.
	//         지워질 값을 보여주면 플레이어가 없는 위험을 관리하게 된다
	// 결과  — 판이 끝난 뒤의 경계도는 정보가 아니라 잔상이다.
	//         "얼마나 시끄러웠는가" 는 결과 화면의 최다 소음 유발자가 더 정확히 말한다
	// 대기  — 페이즈 태그가 비어 있다. 아직 아무 판도 시작되지 않았다
	const bool bRelevant = Phase.MatchesTag(HHTags::Phase_Heist)
						|| Phase.MatchesTag(HHTags::Phase_Escape);

	SetGaugeRelevant(bRelevant,
					 Phase.IsValid() ? *Phase.ToString() : TEXT("(접속 대기)"));
}

void UAlertGaugeWidget::SetGaugeRelevant(bool bRelevant, const TCHAR* Cause)
{
	if (bGaugeRelevant == bRelevant)
	{
		return;
	}
	bGaugeRelevant = bRelevant;

	// 게이지는 "안 보이는 것이 정상" 인 구간이 있는 UI 라, 화면만 봐서는
	// 숨긴 것과 고장난 것을 구별할 수 없다. 사유를 같이 남긴다
	UE_LOG(LogHeavyUI, Log, TEXT("%s: 경계도 게이지 %s (페이즈 %s)"),
		   *GetName(), bGaugeRelevant ? TEXT("표시") : TEXT("숨김"), Cause);

	SetVisibility(bGaugeRelevant ? ESlateVisibility::SelfHitTestInvisible
								 : ESlateVisibility::Collapsed);

	OnGaugeVisibilityChanged(bGaugeRelevant);
}

// ──────────────────────────────────────────────────────────────
// 구독 콜백
// ──────────────────────────────────────────────────────────────

void UAlertGaugeWidget::HandleGaugeChanged(float NewGauge01)
{
	TargetGauge = FMath::Clamp(NewGauge01, 0.f, 1.f);

	if (BarInterpSpeed > 0.f)
	{
		StartInterp();
	}
	else
	{
		DisplayedGauge = TargetGauge;
		ApplyGaugeVisual(DisplayedGauge);
	}

	// BP 훅에는 보간 중인 표시값이 아니라 실제 값을 넘긴다.
	// 연출이 판정과 같은 값을 보고 있어야 한다
	OnGaugeUpdated(TargetGauge);
}

void UAlertGaugeWidget::HandleLevelChanged(EAlertLevel NewLevel, EAlertLevel OldLevel)
{
	ApplyLevelVisual(NewLevel);

	OnLevelUpdated(NewLevel, OldLevel, UUISettings::Get()->GetAlertLevelColor(NewLevel));
}

// ──────────────────────────────────────────────────────────────
// 표시
// ──────────────────────────────────────────────────────────────

void UAlertGaugeWidget::ApplyGaugeVisual(float Gauge01)
{
	if (Bar_Alert)
	{
		Bar_Alert->SetPercent(Gauge01);
	}

	if (Txt_Percent)
	{
		const int32 Percent = FMath::RoundToInt(Gauge01 * 100.f);

		// 보간 중에는 초당 60번 들어오는데 정수 퍼센트는 대부분 그대로다.
		// 바뀔 때만 갱신해 매 스텝 FText 를 새로 만들지 않는다
		if (Percent != LastShownPercent)
		{
			LastShownPercent = Percent;
			Txt_Percent->SetText(FText::Format(
					NSLOCTEXT("HeavyUI", "AlertGaugePercent", "{0}%"), Percent));
		}
	}
}

void UAlertGaugeWidget::ApplyLevelVisual(EAlertLevel NewLevel)
{
	const FLinearColor LevelColor = UUISettings::Get()->GetAlertLevelColor(NewLevel);

	if (Bar_Alert)
	{
		Bar_Alert->SetFillColorAndOpacity(LevelColor);
	}

	if (Txt_Level)
	{
		// 색만으로 단계를 구분하지 않는다. 이름을 항상 같이 띄운다
		Txt_Level->SetText(UUISettings::GetAlertLevelText(NewLevel));
		Txt_Level->SetColorAndOpacity(FSlateColor(LevelColor));
	}

	UpdateAlarmBlink(NewLevel == EAlertLevel::Alarm);
}

void UAlertGaugeWidget::UpdateAlarmBlink(bool bShouldBlink)
{
	// 디자이너 프리뷰에서 애니메이션을 돌리면 편집 중 화면이 계속 깜빡인다
	if (IsDesignTime())
	{
		return;
	}

	if (!AlarmBlink)
	{
		// 경보가 떴는데 점멸이 없다. 화면만 봐서는 "원래 안 깜빡이나?" 로 보여
		// 아무도 문제로 인식하지 못하므로 로그로 깨 준다
		if (bShouldBlink && !bWarnedMissingBlink)
		{
			bWarnedMissingBlink = true;
			UE_LOG(LogHeavyUI, Warning,
				   TEXT("%s: AlarmBlink 애니메이션이 없어 경보 점멸이 재생되지 않는다. "
						"WBP 에 2Hz 로 저작할 것"),
				   *GetName());
		}
		return;
	}

	const float BlinkHz = UUISettings::Get()->AlarmBlinkHz;

	if (!bShouldBlink || BlinkHz <= 0.f)
	{
		StopAnimation(AlarmBlink);
		return;
	}

	// 같은 단계로 다시 들어온 경우다. 처음부터 다시 재생하면 점멸이 눈에 띄게 튄다
	if (IsAnimationPlaying(AlarmBlink))
	{
		return;
	}

	// AlarmBlink 는 2Hz 로 저작돼 있다. 설정값을 반영하려면 재생 속도를 그 비율로 준다 —
	// 4Hz 로 올리면 2배속이 된다. 이 환산을 WBP 가 기억해서 해야 했던 것을 여기로 가져왔다
	PlayAnimation(AlarmBlink, 0.f, /*NumLoopsToPlay=*/0,
				  EUMGSequencePlayMode::Forward, BlinkHz / AuthoredBlinkHz);
}

// ──────────────────────────────────────────────────────────────
// 보간
// ──────────────────────────────────────────────────────────────

void UAlertGaugeWidget::StartInterp()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(InterpHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
			InterpHandle, this, &UAlertGaugeWidget::StepInterp, InterpInterval, true);
}

void UAlertGaugeWidget::StopInterp()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InterpHandle);
	}
}

void UAlertGaugeWidget::StepInterp()
{
	DisplayedGauge = FMath::FInterpTo(DisplayedGauge, TargetGauge, InterpInterval, BarInterpSpeed);

	// 남은 거리에 비례해 좁히는 방식이라 목표에 정확히 닿지 않는다.
	// 스냅하고 꺼 주지 않으면 타이머가 영원히 돈다
	if (FMath::IsNearlyEqual(DisplayedGauge, TargetGauge, InterpSnapTolerance))
	{
		DisplayedGauge = TargetGauge;
		StopInterp();
	}

	ApplyGaugeVisual(DisplayedGauge);
}

// ──────────────────────────────────────────────────────────────
// 조회
// ──────────────────────────────────────────────────────────────

float UAlertGaugeWidget::GetGauge01() const
{
	const UAlertComponent* Alert = BoundAlert.Get();
	return Alert ? Alert->GetAlertGauge01() : 0.f;
}

EAlertLevel UAlertGaugeWidget::GetAlertLevel() const
{
	const UAlertComponent* Alert = BoundAlert.Get();
	return Alert ? Alert->GetAlertLevel() : EAlertLevel::Calm;
}

FLinearColor UAlertGaugeWidget::GetLevelColor() const
{
	return UUISettings::Get()->GetAlertLevelColor(GetAlertLevel());
}

FText UAlertGaugeWidget::GetLevelText() const
{
	return UUISettings::GetAlertLevelText(GetAlertLevel());
}

bool UAlertGaugeWidget::IsAlarmed() const
{
	const UAlertComponent* Alert = BoundAlert.Get();
	return Alert && Alert->IsAlarmed();
}
