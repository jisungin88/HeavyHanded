#include "UI/HeistHUDWidget.h"

#include "Animation/WidgetAnimation.h"
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
	 *
	 * [왜 사유까지 받는가] 도주는 같은 90초라도 어떻게 들어왔는지가 다르다.
	 *   경보로 넘어왔으면 쫓기는 시간이고(기획서 3장 — 경찰 출동까지 90초),
	 *   제한 시간이 다 돼서 넘어왔으면 그냥 남은 시간이다. 플레이어가 화면만 보고
	 *   "들켰나?" 를 판단할 수 있어야 하는데, 그 사실은 페이즈 태그에 없고
	 *   EHeistPhaseReason 에만 있다. 이유가 복제되는 것이 이것 때문이다.
	 */
	FText PhaseToText(const FGameplayTag& Phase, EHeistPhaseReason Reason)
	{
		if (Phase.MatchesTagExact(HHTags::Phase_Prep))   { return LOCTEXT("PhasePrep",   "준비"); }
		if (Phase.MatchesTagExact(HHTags::Phase_Heist))  { return LOCTEXT("PhaseHeist",  "본 작업"); }
		if (Phase.MatchesTagExact(HHTags::Phase_Result)) { return LOCTEXT("PhaseResult", "결과"); }

		if (Phase.MatchesTagExact(HHTags::Phase_Escape))
		{
			// Cheat 로 넘어온 경우도 여기로 떨어진다. 치트는 시간 만료와 같은 화면이면 된다 —
			// 없는 경보를 띄우는 것보다 낫고, 치트라는 사실은 로그에 이미 남는다
			return Reason == EHeistPhaseReason::Alarm
				? LOCTEXT("PhaseEscapeAlarm",  "경찰 도착까지")
				: LOCTEXT("PhaseEscapeTimeUp", "탈출까지");
		}

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
}

void UHeistHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// BindWidgetOptional 은 없어도 컴파일이 통과한다. 그래서 이름이 틀렸을 때
	// 화면에 아무것도 안 뜨는 것으로만 드러나고, 그 모습은 "아직 안 만든 것" 과 같다.
	// 없다는 사실 자체를 남겨야 화면을 안 보고도 구분할 수 있다
	if (!Txt_Phase)
	{
		UE_LOG(LogHeavyUI, Log,
			   TEXT("%s: WBP 에 Txt_Phase 가 없어 페이즈 문구를 표시하지 않는다"), *GetName());
	}

	if (!Txt_Objective)
	{
		UE_LOG(LogHeavyUI, Log,
			   TEXT("%s: WBP 에 Txt_Objective 가 없어 목표 금액을 표시하지 않는다"), *GetName());
	}

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
	const FText Label = PhaseToText(NewPhase, Reason);

	if (Txt_Phase)
	{
		Txt_Phase->SetText(Label);
	}

	// 화면에 실제로 무엇을 썼는지 남긴다. 페이즈와 사유는 코어 루프가 이미 찍고 있지만,
	// 그것이 어떤 문구가 됐는지는 여기서만 알 수 있다 — 사유별 분기가 맞게 갈렸는지는
	// 화면을 보지 않고 확인할 방법이 이것뿐이다
	UE_LOG(LogHeavyUI, Log, TEXT("%s: 페이즈 %s (사유: %s) → 문구 \"%s\"%s"),
		   *GetName(),
		   NewPhase.IsValid() ? *NewPhase.ToString() : TEXT("(없음)"),
		   HeistPhase::ToString(Reason),
		   *Label.ToString(),
		   Txt_Phase ? TEXT("") : TEXT(" (Txt_Phase 없음 — 표시되지 않음)"));

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

	const AHeistGameState* GS = BoundState.Get();

	// 결과 페이즈에도 카운트다운은 있다 (UHeistSettings::ResultSeconds — 아무도 확인을
	// 누르지 않았을 때의 체류 시간). 그건 결과 화면이 자기 자리에 그릴 값이지
	// 미션 타이머가 아니다. 여기서 걸러내지 않으면 판이 끝나는 순간
	// 미션 타이머 자리가 0:30 으로 되살아난다
	const bool bResultPhase = GS && GS->IsPhase(HHTags::Phase_Result);

	float Remaining = 0.f;
	if (bResultPhase || !TryGetRemainingSeconds(Remaining))
	{
		// 보여줄 카운트다운이 없는 구간(결과 · 접속 대기)이다. -1 이나 0:00 을 찍지 않는다 —
		// 반환 타입이 그 구분을 강제하는 이유가 이것이다
		Txt_Timer->SetVisibility(ESlateVisibility::Collapsed);
		LastShownSeconds = INDEX_NONE;
		SetUrgent(false, TEXT("카운트다운 없는 구간"));
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

	// 도주는 진입한 순간부터 위급하다. 남은 시간으로만 판단하면 90초 도주의 앞 60초가
	// 본 작업과 똑같은 화면이 된다 — 기획서 3장이 "장르 전환" 이라고 부른 구간인데
	// 화면에는 아무 일도 일어나지 않는 셈이다.
	//
	// UrgentSeconds 는 그래서 Prep · Heist 용 임계값으로만 남는다
	const bool bEscapePhase = GS && GS->IsPhase(HHTags::Phase_Escape);
	const bool bLowTime     = (UrgentSeconds > 0.f && Remaining <= UrgentSeconds);

	// 어느 조건으로 켜졌는지까지 넘긴다. 빨간 타이머만 보고는 "도주라서" 와
	// "시간이 없어서" 를 구분할 수 없고, 그 둘이 갈리는 것이 이 기능의 전부다
	SetUrgent(bEscapePhase || bLowTime,
			  bEscapePhase ? TEXT("도주 페이즈") : (bLowTime ? TEXT("남은 시간 부족") : TEXT("해제")));
}

void UHeistHUDWidget::ApplyObjective(int32 LoadedValue, int32 TargetValue)
{
	if (Txt_Objective)
	{
		Txt_Objective->SetText(FText::Format(
				LOCTEXT("ObjectiveFormat", "${0} / ${1}"),
				FText::AsNumber(LoadedValue),
				FText::AsNumber(TargetValue)));

		// 목표를 채우면 금색으로 바꾼다. 진행도 바가 없으므로 색이 유일한 신호다 —
		// 초과 적재는 숫자로만 드러나서 눈에 잘 띄지 않는다
		const EUIColorToken Token = (TargetValue > 0 && LoadedValue >= TargetValue)
			? EUIColorToken::Gold
			: EUIColorToken::Money;
		Txt_Objective->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(Token)));
	}
}

void UHeistHUDWidget::SetUrgent(bool bNewUrgent, const TCHAR* Cause)
{
	if (bUrgent == bNewUrgent)
	{
		return;
	}
	bUrgent = bNewUrgent;

	// 상태가 바뀔 때만 찍힌다 — RefreshTimer 는 0.1초마다 도므로 조건 없이 로그하면
	// 미션 내내 초당 열 줄이 쌓인다 (문서 09 — 반복 경로에서는 한 번만 찍는다)
	UE_LOG(LogHeavyUI, Log, TEXT("%s: 타이머 경고 %s (%s)"),
		   *GetName(), bUrgent ? TEXT("켬") : TEXT("끔"), Cause);

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
	const AHeistGameState* GS = BoundState.Get();

	// 못 붙었으면 페이즈도 비어 있어서 사유가 무엇이든 "대기" 로 떨어진다
	return PhaseToText(GetCurrentPhase(),
					   GS ? GS->GetPhaseReason() : EHeistPhaseReason::Scheduled);
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
