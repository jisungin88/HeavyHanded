#include "UI/HeistHUDWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "UI/HeavyUILog.h"
#include "UI/UISettings.h"

#define LOCTEXT_NAMESPACE "HeavyUI"

namespace
{
	/**
	 * 페이즈 화면 문구.
	 *
	 * EAlertLevel 은 enum 의 UMETA(DisplayName) 에서 이름을 꺼내 쓸 수 있었지만
	 * 페이즈는 GameplayTag 라 그럴 메타데이터가 없다. .ini 의 DevComment 는
	 * 에디터 전용이라 런타임에 읽을 것이 못 된다. 그래서 여기 한 곳에 둔다.
	 *
	 * 기획자가 문구를 직접 만져야 할 만큼 자주 바뀌면 UUISettings 로 옮긴다.
	 */
	FText PhaseToText(const FGameplayTag& Phase)
	{
		if (Phase.MatchesTagExact(HHTags::Phase_Prep))   { return LOCTEXT("PhasePrep",   "준비"); }
		if (Phase.MatchesTagExact(HHTags::Phase_Heist))  { return LOCTEXT("PhaseHeist",  "본 작업"); }
		if (Phase.MatchesTagExact(HHTags::Phase_Escape)) { return LOCTEXT("PhaseEscape", "탈출"); }
		if (Phase.MatchesTagExact(HHTags::Phase_Result)) { return LOCTEXT("PhaseResult", "결과"); }

		// 접속 대기 중에는 페이즈가 비어 있다
		return LOCTEXT("PhaseWaiting", "대기");
	}
}

void UHeistHUDWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// PreConstruct 는 디자이너에서도 실행된다. 토큰을 여기서 적용해야 편집 중에도
	// 실제 색 · 폰트가 보이고, UMG 에 색이 구워지는 것을 막는다 (UISettings.h 주석)
	if (Txt_Timer)
	{
		Txt_Timer->SetFont(UUISettings::GetUIFont(EUIFontToken::Timer));
		Txt_Timer->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_Phase)
	{
		Txt_Phase->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
		Txt_Phase->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextSecondary)));
	}

	if (Txt_Objective)
	{
		Txt_Objective->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Objective->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::Money)));
	}

	if (Bar_Objective)
	{
		Bar_Objective->SetFillColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::Money));
	}
}

void UHeistHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 붙기 전까지는 숨겨 둔다. 빈 "텍스트 블록" 이 화면에 남는 것보다 낫다
	SetHeistWidgetsVisible(false);

	BindElapsed = 0.f;
	TryBind();
}

void UHeistHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
		World->GetTimerManager().ClearTimer(TimerTickHandle);
	}

	// 구독을 안 풀면 위젯이 사라진 뒤에도 델리게이트에 남는다
	if (AHeistGameState* GS = BoundState.Get())
	{
		GS->OnPhaseChanged.RemoveDynamic(this, &UHeistHUDWidget::HandlePhaseChanged);
		GS->OnLoadedValueChanged.RemoveDynamic(this, &UHeistHUDWidget::HandleLoadedValueChanged);
	}
	BoundState = nullptr;

	Super::NativeDestruct();
}

void UHeistHUDWidget::TryBind()
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

	AHeistGameState* GS = AHeistGameState::Get(this);
	if (!GS)
	{
		BindElapsed += BindRetryInterval;

		// 작업 레벨이 아니면 영원히 안 온다. 무한 재시도로 두면 GuardTest 같은 맵에서
		// 아무 일도 안 하는 타이머가 계속 돈다
		if (BindElapsed >= BindGiveUpSeconds)
		{
			if (!bWarnedNoHeistState)
			{
				bWarnedNoHeistState = true;
				UE_LOG(LogHeavyUI, Log,
					   TEXT("%s: AHeistGameState 가 없어 타이머 · 목표 금액을 숨긴다. "
							"작업 레벨이 아니면 정상이다"),
					   *GetName());
			}
			return;
		}

		// 클라이언트에서는 GameState 가 복제로 뒤늦게 도착한다
		World->GetTimerManager().SetTimer(
				BindRetryHandle, this, &UHeistHUDWidget::TryBind, BindRetryInterval, false);
		return;
	}

	BoundState = GS;

	GS->OnPhaseChanged.AddDynamic(this, &UHeistHUDWidget::HandlePhaseChanged);
	GS->OnLoadedValueChanged.AddDynamic(this, &UHeistHUDWidget::HandleLoadedValueChanged);

	SetHeistWidgetsVisible(true);

	// 구독 시점의 값으로 한 번 그린다. 안 하면 다음 전환까지 빈 화면이 보인다
	HandlePhaseChanged(GS->GetCurrentPhase(), FGameplayTag(), GS->GetPhaseReason());
	ApplyObjective(GS->GetLoadedValue(), GS->GetTargetValue());

	// 남은 시간은 복제되지 않는다 — 끝나는 시각만 오고 남은 초는 각자 계산한다.
	// 그래서 구독이 아니라 주기 갱신이다
	World->GetTimerManager().SetTimer(
			TimerTickHandle, this, &UHeistHUDWidget::RefreshTimer, TimerTickInterval, true);

	RefreshTimer();
}

// ──────────────────────────────────────────────────────────────
// 구독 콜백
// ──────────────────────────────────────────────────────────────

void UHeistHUDWidget::HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason)
{
	if (Txt_Phase)
	{
		Txt_Phase->SetText(PhaseToText(NewPhase));
	}

	// 페이즈가 바뀌면 카운트다운 유무도 바뀐다. 다음 주기를 기다리지 않고 바로 반영한다
	LastShownSeconds = INDEX_NONE;
	RefreshTimer();

	OnPhaseUpdated(NewPhase, OldPhase);
}

void UHeistHUDWidget::HandleLoadedValueChanged(int32 LoadedValue, int32 TargetValue)
{
	ApplyObjective(LoadedValue, TargetValue);

	OnObjectiveUpdated(LoadedValue, TargetValue);
}

// ──────────────────────────────────────────────────────────────
// 표시
// ──────────────────────────────────────────────────────────────

void UHeistHUDWidget::RefreshTimer()
{
	if (!Txt_Timer)
	{
		return;
	}

	float Remaining = 0.f;
	if (!TryGetRemainingSeconds(Remaining))
	{
		// 카운트다운이 없는 구간(결과 · 접속 대기)이다. -1 이나 0:00 을 찍지 않는다 —
		// 반환 타입이 그 구분을 강제하는 이유가 이것이다
		Txt_Timer->SetVisibility(ESlateVisibility::Collapsed);
		LastShownSeconds = INDEX_NONE;
		SetUrgent(false);
		return;
	}

	Txt_Timer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 올림이라 마지막 1초가 0:00 으로 먼저 넘어가지 않는다.
	// 내림으로 하면 아직 0.9초 남았는데 화면이 0:00 이 된다
	const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(Remaining));

	if (TotalSeconds != LastShownSeconds)
	{
		LastShownSeconds = TotalSeconds;

		// 초는 항상 두 자리다 — "6:5" 가 아니라 "6:05".
		// 자릿수가 오갈 때마다 글자 폭이 흔들리면 시선이 그쪽으로 끌린다
		FNumberFormattingOptions SecondsFormat;
		SecondsFormat.MinimumIntegralDigits = 2;

		Txt_Timer->SetText(FText::Format(
				LOCTEXT("TimerFormat", "{0}:{1}"),
				FText::AsNumber(TotalSeconds / 60),
				FText::AsNumber(TotalSeconds % 60, &SecondsFormat)));
	}

	SetUrgent(UrgentSeconds > 0.f && Remaining <= UrgentSeconds);
}

void UHeistHUDWidget::ApplyObjective(int32 LoadedValue, int32 TargetValue)
{
	if (Bar_Objective)
	{
		// 목표가 0 인 판(설정 누락)에서 0 나누기를 하지 않는다.
		// 초과 적재도 있으므로 1 로 자른다
		const float Ratio = (TargetValue > 0)
			? FMath::Clamp(static_cast<float>(LoadedValue) / static_cast<float>(TargetValue), 0.f, 1.f)
			: 0.f;

		Bar_Objective->SetPercent(Ratio);

		// 목표를 채우면 금색으로 바꾼다. 초과분은 바가 더 안 차므로 색이 유일한 신호다
		const EUIColorToken Token = (TargetValue > 0 && LoadedValue >= TargetValue)
			? EUIColorToken::Gold
			: EUIColorToken::Money;
		Bar_Objective->SetFillColorAndOpacity(UUISettings::GetUIColor(Token));
	}

	if (Txt_Objective)
	{
		Txt_Objective->SetText(FText::Format(
				LOCTEXT("ObjectiveFormat", "${0} / ${1}"),
				FText::AsNumber(LoadedValue),
				FText::AsNumber(TargetValue)));
	}
}

void UHeistHUDWidget::SetUrgent(bool bNewUrgent)
{
	if (bUrgent == bNewUrgent)
	{
		return;
	}
	bUrgent = bNewUrgent;

	if (Txt_Timer)
	{
		// 경보 색을 그대로 쓴다. "시간이 없다" 와 "들켰다" 는 다른 사건이지만
		// 플레이어에게는 둘 다 같은 종류의 위급함이고, 색을 새로 만들면 팔레트만 늘어난다
		const FLinearColor Color = bUrgent
			? UUISettings::Get()->AlarmColor
			: UUISettings::GetUIColor(EUIColorToken::TextPrimary);

		Txt_Timer->SetColorAndOpacity(FSlateColor(Color));
	}

	if (UrgentPulse && !IsDesignTime())
	{
		if (bUrgent)
		{
			if (!IsAnimationPlaying(UrgentPulse))
			{
				PlayAnimation(UrgentPulse, 0.f, /*NumLoopsToPlay=*/0);
			}
		}
		else
		{
			StopAnimation(UrgentPulse);
		}
	}

	OnUrgentChanged(bUrgent);
}

void UHeistHUDWidget::SetHeistWidgetsVisible(bool bVisible)
{
	const ESlateVisibility Vis = bVisible ? ESlateVisibility::SelfHitTestInvisible
										  : ESlateVisibility::Collapsed;

	if (Txt_Timer)     { Txt_Timer->SetVisibility(Vis); }
	if (Txt_Phase)     { Txt_Phase->SetVisibility(Vis); }
	if (Txt_Objective) { Txt_Objective->SetVisibility(Vis); }
	if (Bar_Objective) { Bar_Objective->SetVisibility(Vis); }
}

// ──────────────────────────────────────────────────────────────
// 조회
// ──────────────────────────────────────────────────────────────

bool UHeistHUDWidget::TryGetRemainingSeconds(float& OutSeconds) const
{
	const AHeistGameState* GS = BoundState.Get();
	return GS && GS->TryGetPhaseRemainingSeconds(OutSeconds);
}

FGameplayTag UHeistHUDWidget::GetCurrentPhase() const
{
	const AHeistGameState* GS = BoundState.Get();
	return GS ? GS->GetCurrentPhase() : FGameplayTag();
}

FText UHeistHUDWidget::GetPhaseLabel() const
{
	return PhaseToText(GetCurrentPhase());
}

int32 UHeistHUDWidget::GetLoadedValue() const
{
	const AHeistGameState* GS = BoundState.Get();
	return GS ? GS->GetLoadedValue() : 0;
}

int32 UHeistHUDWidget::GetTargetValue() const
{
	const AHeistGameState* GS = BoundState.Get();
	return GS ? GS->GetTargetValue() : 0;
}

#undef LOCTEXT_NAMESPACE
