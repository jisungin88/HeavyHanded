#include "UI/HeistHUDWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistSettings.h"            // PrepSeconds — "작전 준비 45초" 의 45
#include "UI/HeavyUILog.h"
#include "UI/UISettings.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"

#include "Character/BaseCharacter.h"   // GetHeldActor — 소지 슬롯이 읽는 유일한 진입점
#include "Loot/LootBase.h"             // 이름 · 무게 · 가치. ICarryable 에는 이 셋이 없다

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

namespace
{
	/** 소지 슬롯에 그릴 값 한 묶음 */
	struct FHeldDisplay
	{
		FText Name;
		float MassKg = 0.f;
		int32 Value = 0;
		bool  bHasValue = false;
		FGameplayTagContainer TypeTags;
	};

	/**
	 * 손에 든 액터에서 화면에 쓸 값을 뽑는다.
	 *
	 * [타입을 보는 곳은 여기 하나다] 기획상 이 슬롯에는 노획물과 장비가 같이 들어온다.
	 *   장비(Equipment)는 아직 코드가 없어 노획물 갈래만 채워져 있다 — 생기면 아래에
	 *   분기 하나만 늘리면 되고 위젯 쪽은 건드릴 필요가 없다.
	 *
	 *   ICarryable 로 통일하지 않은 이유는 그 인터페이스에 이름 · 무게 · 가치가 없기 때문이다.
	 *   운반 판정용(GetRequiredCarriers · GetCarrySpeedMultiplier · CanBeCarriedBy)만 들어 있다.
	 *   장비를 붙일 때 인터페이스를 넓힐지는 사전 합의가 필요하다 (규약 06 — 여러 영역에
	 *   걸치는 새 인터페이스).
	 *
	 * @return 그릴 것이 있으면 true
	 */
	bool BuildHeldDisplay(const AActor* Held, FHeldDisplay& Out)
	{
		if (!IsValid(Held))
		{
			return false;
		}

		if (const ALootBase* Loot = Cast<ALootBase>(Held))
		{
			Out.Name      = Loot->GetDisplayName();
			Out.MassKg    = Loot->GetPhysicsData().MassKg;
			Out.Value     = Loot->GetCurrentValue();
			Out.bHasValue = true;
			Loot->GetOwnedGameplayTags(Out.TypeTags);

			// 카탈로그 행을 지정하지 않은 노획물은 이름이 비어 있다. 여기서 GetName() 으로
			// 때우면 "BP_Loot_Fragile_C_0" 이 화면에 그대로 뜬다 (LootBase.h GetDisplayName 주석)
			if (Out.Name.IsEmpty())
			{
				Out.Name = LOCTEXT("HeldUnnamed", "노획물");
			}

			return true;
		}

		// TODO(장비): Equipment 가 생기면 여기에 갈래를 하나 더 둔다.
		//   Source/HeavyHanded/{Public,Private}/Equipment/ 는 현재 비어 있다 (기획서 7장 미착수)

		// 노획물도 장비도 아닌 것을 들고 있다. 클래스 이름이라도 남긴다 —
		// 슬롯이 통째로 비면 "안 들고 있는 것" 과 구분이 안 된다
		Out.Name = FText::FromString(Held->GetName());
		return true;
	}
}

#if !UE_BUILD_SHIPPING
namespace
{
	/** 치트가 찾아갈 위젯 목록. StaminaVignetteWidget 의 GLiveStaminaVignettes 와 같은 방식이다 */
	TArray<TWeakObjectPtr<UHeistHUDWidget>> GLiveHeistHUDs;
}
#endif

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

	if (Txt_PrepCenterValue)
	{
		// 숫자 폰트(Oswald)다. "45초" · "시작!" 의 한글은 폰트 에셋의 Noto 폴백이 받는다
		FSlateFontInfo BigFont = UUISettings::GetUIFont(EUIFontToken::Timer);
		BigFont.Size *= PrepCenterFontScale;

		Txt_PrepCenterValue->SetFont(BigFont);
		Txt_PrepCenterValue->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_PrepCenterLabel)
	{
		Txt_PrepCenterLabel->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_PrepCenterLabel->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_PrepMini)
	{
		Txt_PrepMini->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_PrepMini->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_Objective)
	{
		Txt_Objective->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Objective->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::Money)));
	}

	if (Txt_HeldName)
	{
		// 슬롯 전용 토큰을 새로 만들지 않고 기본 토큰에 배율만 곱한다
		// (UUISettings::HeldSlotFontScale — 폰트 3단계를 유지하기 위해서다)
		FSlateFontInfo NameFont = UUISettings::GetUIFont(EUIFontToken::Value);
		NameFont.Size *= UUISettings::Get()->HeldSlotFontScale;

		Txt_HeldName->SetFont(NameFont);
		Txt_HeldName->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_HeldInfo)
	{
		FSlateFontInfo InfoFont = UUISettings::GetUIFont(EUIFontToken::Label);
		InfoFont.Size *= UUISettings::Get()->HeldSlotFontScale;

		Txt_HeldInfo->SetFont(InfoFont);
		Txt_HeldInfo->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextSecondary)));
	}

	if (Bar_HeldWeight)
	{
		// 무게는 금액이 아니다. 돈 색(Money)을 쓰면 목표 금액과 같은 뜻으로 읽힌다
		Bar_HeldWeight->SetFillColorAndOpacity(UUISettings::GetUIColor(EUIColorToken::Gold));
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

#if !UE_BUILD_SHIPPING
	GLiveHeistHUDs.RemoveAll([](const TWeakObjectPtr<UHeistHUDWidget>& Weak) { return !Weak.IsValid(); });
	GLiveHeistHUDs.AddUnique(this);
#endif

	// 위급이 풀릴 때 되돌아갈 색. SetUrgent 가 한 번이라도 돌기 전에 읽어야 한다
	if (Img_Plate)       { PlateNormalColor       = Img_Plate->GetColorAndOpacity(); }
	if (Img_PlateShadow) { PlateShadowNormalColor = Img_PlateShadow->GetColorAndOpacity(); }

	// 붙기 전까지는 숨겨 둔다. 빈 "텍스트 블록" 이 화면에 남는 것보다 낫다
	SetHeistWidgetsVisible(false);

	BindElapsed = 0.f;
	TryBind();

	// 소지 슬롯은 AHeistGameState 와 무관하다 — 작업 레벨이 아닌 테스트 맵에서도 떠야 하므로
	// TryBind 성공 여부와 상관없이 따로 건다.
	//
	// [여기서 위젯을 직접 만지지 않는 이유] 새 BindWidget 을 넣은 직후 BP 컴파일이 꼬여 있으면
	// 포인터가 쓰레기값이다. 첫 접근을 타이머 첫 틱으로 미루면 크래시 대신 로그를 볼 기회가
	// 남는다 (2026-08-25 Bar_Weight 이름 충돌로 두 번 크래시)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
				HeldTickHandle, this, &UHeistHUDWidget::RefreshHeldSlot, HeldTickInterval, true);
	}
}

void UHeistHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
		World->GetTimerManager().ClearTimer(TimerTickHandle);
		World->GetTimerManager().ClearTimer(HeldTickHandle);
		World->GetTimerManager().ClearTimer(MoneyInterpHandle);
		World->GetTimerManager().ClearTimer(DebugPrepHandle);
	}

#if !UE_BUILD_SHIPPING
	GLiveHeistHUDs.Remove(this);
#endif

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

	// 구독 시점의 값으로 한 번 그린다. 안 하면 다음 전환까지 빈 화면이 보인다.
	//
	// 여기만 롤업 없이 즉시 찍는다 — 이미 $18,400 이 실려 있는 판에 늦게 들어왔는데
	// 0 부터 굴러가면 방금 자기가 실은 것처럼 보인다
	HandlePhaseChanged(GS->GetCurrentPhase(), FGameplayTag(), GS->GetPhaseReason());
	ApplyObjective(GS->GetLoadedValue(), GS->GetTargetValue(), /*bImmediate=*/true);

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

	// 준비에서 본 작업으로 넘어온 순간에만 "시작!" 을 띄운다.
	// 늦게 들어와 최초 바인딩으로 불린 경우(OldPhase 가 비어 있다)는 이미 시작한 판이다
	StartBannerUntil = -1.f;
	if (StartBannerSeconds > 0.f
		&& NewPhase.MatchesTagExact(HHTags::Phase_Heist)
		&& OldPhase.MatchesTagExact(HHTags::Phase_Prep))
	{
		if (const UWorld* World = GetWorld())
		{
			StartBannerUntil = World->GetTimeSeconds() + StartBannerSeconds;
		}
	}

	// 페이즈가 바뀌면 카운트다운 유무도 바뀐다. 다음 주기를 기다리지 않고 바로 반영한다
	LastShownSeconds = INDEX_NONE;
	RefreshTimer();

	OnPhaseUpdated(NewPhase, OldPhase);
}

void UHeistHUDWidget::HandleLoadedValueChanged(int32 LoadedValue, int32 TargetValue)
{
	ApplyObjective(LoadedValue, TargetValue, /*bImmediate=*/false);

	// 훅은 실제 값이 바뀐 이 순간에만 간다. 롤업 프레임마다 부르면 WBP 의
	// 펀치 애니메이션이 초당 60번 처음부터 다시 재생돼 멈춘 것처럼 보인다
	OnObjectiveUpdated(LoadedValue, TargetValue);
}

// ──────────────────────────────────────────────────────────────
// 표시
// ──────────────────────────────────────────────────────────────

namespace
{
	/**
	 * "6:05" 형식. 초는 항상 두 자리다 — "6:5" 가 아니라 "6:05".
	 * 자릿수가 오갈 때마다 글자 폭이 흔들리면 시선이 그쪽으로 끌린다
	 */
	FText FormatClock(int32 TotalSeconds)
	{
		FNumberFormattingOptions SecondsFormat;
		SecondsFormat.MinimumIntegralDigits = 2;

		return FText::Format(
				LOCTEXT("TimerFormat", "{0}:{1}"),
				FText::AsNumber(TotalSeconds / 60),
				FText::AsNumber(TotalSeconds % 60, &SecondsFormat));
	}
}

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

	// 준비 시간(45초)에도 카운트다운은 있지만 미션 타이머 자리에는 띄우지 않는다.
	// 타이머는 본 작업 시작부터 나온다
	// 흉내 준비(치트) 중에는 진짜 페이즈가 무엇이든 준비 화면으로 둔다 — 판이 떠 있으면 확인이 안 된다
	const bool bPrepPhase = bDebugPrepCounting || (GS && GS->IsPhase(HHTags::Phase_Prep));

	// 준비 시간은 판 대신 가운데 카운트다운이 맡는다. 흉내 중이면 StepDebugPrep 이 대신 그린다
	if (!DebugPrepHandle.IsValid())
	{
		float PrepRemaining = 0.f;
		const bool bPrepCountdown = bPrepPhase && TryGetRemainingSeconds(PrepRemaining);
		RefreshPrepCountdown(bPrepCountdown, PrepRemaining);
	}

	float Remaining = 0.f;
	if (bResultPhase || bPrepPhase || !TryGetRemainingSeconds(Remaining))
	{
		// 보여줄 카운트다운이 없는 구간(준비 · 결과 · 접속 대기)이다. -1 이나 0:00 을 찍지 않는다 —
		// 반환 타입이 그 구분을 강제하는 이유가 이것이다
		SetTimerVisible(false);
		LastShownSeconds = INDEX_NONE;
		SetUrgent(false, TEXT("카운트다운 없는 구간"));
		return;
	}

	SetTimerVisible(true);

	// 올림이라 마지막 1초가 0:00 으로 먼저 넘어가지 않는다.
	// 내림으로 하면 아직 0.9초 남았는데 화면이 0:00 이 된다
	const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(Remaining));

	if (TotalSeconds != LastShownSeconds)
	{
		LastShownSeconds = TotalSeconds;
		Txt_Timer->SetText(FormatClock(TotalSeconds));
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

void UHeistHUDWidget::ApplyObjective(int32 LoadedValue, int32 TargetValue, bool bImmediate)
{
	TargetLoaded = LoadedValue;
	TargetGoal   = TargetValue;

	if (bImmediate || MoneyInterpSpeed <= 0.f)
	{
		StopMoneyInterp();
		DisplayedLoaded = static_cast<float>(LoadedValue);
		DrawObjective();
		return;
	}

	// 롤업 중에 또 실려도 표시값은 건드리지 않는다 — 목표만 갈아끼우면
	// 지금 찍혀 있는 숫자에서 이어서 굴러간다. 되감기면 적재가 취소된 것처럼 보인다
	StartMoneyInterp();
}

void UHeistHUDWidget::DrawObjective()
{
	if (!Txt_Objective)
	{
		return;
	}

	const int32 Shown = FMath::RoundToInt(DisplayedLoaded);
	if (Shown == LastShownLoaded && TargetGoal == LastShownGoal)
	{
		return;
	}
	LastShownLoaded = Shown;
	LastShownGoal   = TargetGoal;

	Txt_Objective->SetText(FText::Format(
			LOCTEXT("ObjectiveFormat", "${0} / ${1}"),
			FText::AsNumber(Shown),
			FText::AsNumber(TargetGoal)));

	// 목표를 채우면 금색으로 바꾼다. 진행도 바가 없으므로 색이 유일한 신호다 —
	// 초과 적재는 숫자로만 드러나서 눈에 잘 띄지 않는다.
	//
	// 기준을 실제 값이 아니라 표시값으로 잡은 것은 의도다. 숫자가 목표에 닿는
	// 그 순간에 색이 같이 바뀌어야 한다 — 실제 값 기준으로 하면 아직 $30,000 이
	// 찍혀 있는데 글자만 금색이 된다
	const EUIColorToken Token = (TargetGoal > 0 && Shown >= TargetGoal)
		? EUIColorToken::Gold
		: EUIColorToken::Money;
	Txt_Objective->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(Token)));
}

void UHeistHUDWidget::StartMoneyInterp()
{
	// 이미 도착해 있으면 타이머를 켤 이유가 없다 (같은 값이 또 오는 경우)
	if (FMath::Abs(DisplayedLoaded - static_cast<float>(TargetLoaded)) <= MoneySnapTolerance)
	{
		DisplayedLoaded = static_cast<float>(TargetLoaded);
		DrawObjective();
		return;
	}

	if (MoneyInterpHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
				MoneyInterpHandle, this, &UHeistHUDWidget::StepMoneyInterp, MoneyInterpInterval, true);
	}
	else
	{
		// 월드가 없으면 굴릴 방법이 없다. 조용히 안 굴러가는 대신 숫자만이라도 맞춘다
		DisplayedLoaded = static_cast<float>(TargetLoaded);
		DrawObjective();
	}
}

void UHeistHUDWidget::StepMoneyInterp()
{
	DisplayedLoaded = FMath::FInterpTo(
			DisplayedLoaded, static_cast<float>(TargetLoaded), MoneyInterpInterval, MoneyInterpSpeed);

	// 남은 차액에 비례해 좁히는 방식이라 목표에 정확히 닿지 않는다.
	// 스냅하고 꺼 주지 않으면 타이머가 영원히 돈다
	if (FMath::Abs(DisplayedLoaded - static_cast<float>(TargetLoaded)) <= MoneySnapTolerance)
	{
		DisplayedLoaded = static_cast<float>(TargetLoaded);
		StopMoneyInterp();
	}

	DrawObjective();
}

void UHeistHUDWidget::StopMoneyInterp()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoneyInterpHandle);
	}
	MoneyInterpHandle.Invalidate();
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

	// 경보 색을 그대로 쓴다. "시간이 없다" 와 "들켰다" 는 다른 사건이지만
	// 플레이어에게는 둘 다 같은 종류의 위급함이고, 색을 새로 만들면 팔레트만 늘어난다
	const FLinearColor AlarmColor = UUISettings::Get()->AlarmColor;

	if (Img_Plate)
	{
		// 판이 있으면 판이 빨개지고 글자는 흰색으로 남는다
		Img_Plate->SetColorAndOpacity(bUrgent ? AlarmColor : PlateNormalColor);

		if (Img_PlateShadow)
		{
			FLinearColor ShadowColor = AlarmColor * UrgentShadowScale;
			ShadowColor.A = AlarmColor.A;
			Img_PlateShadow->SetColorAndOpacity(bUrgent ? ShadowColor : PlateShadowNormalColor);
		}
	}
	else if (Txt_Timer)
	{
		// 판 없이 글자만 쓰는 WBP — 글자 색으로만 알린다
		Txt_Timer->SetColorAndOpacity(FSlateColor(bUrgent
			? AlarmColor
			: UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
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

	SetTimerVisible(bVisible);
	if (Txt_Phase)     { Txt_Phase->SetVisibility(Vis); }
	if (Txt_Objective) { Txt_Objective->SetVisibility(Vis); }

	// 준비 카운트다운은 켜는 쪽을 RefreshTimer 가 정한다. 여기서는 끄기만 한다
	if (!bVisible)
	{
		HidePrepCenter();
		SetPrepMini(INDEX_NONE);
	}
}

void UHeistHUDWidget::SetTimerVisible(bool bVisible)
{
	const ESlateVisibility Vis = bVisible ? ESlateVisibility::SelfHitTestInvisible
										  : ESlateVisibility::Collapsed;

	// 판이 있으면 판째로 숨긴다. 글자도 같이 맞춰 두는 것은 판 없이 쓰는 WBP 때문이다
	if (Panel_Timer) { Panel_Timer->SetVisibility(Vis); }
	if (Txt_Timer)   { Txt_Timer->SetVisibility(Vis); }
}

// ──────────────────────────────────────────────────────────────
// 준비 카운트다운
// ──────────────────────────────────────────────────────────────

void UHeistHUDWidget::RefreshPrepCountdown(bool bPrep, float Remaining)
{
	if (bPrep)
	{
		// 0 은 찍지 않는다. 남은 시간이 0 이 된 뒤 페이즈가 넘어오기까지 한두 틱이 비는데,
		// 그 사이 "0" 이 보였다가 "시작!" 으로 바뀌면 한 박자 늘어진다
		const int32 Seconds = FMath::Max(1, FMath::CeilToInt(Remaining));
		const float Total   = UHeistSettings::Get()->PrepSeconds;

		// ① 알림 — 준비가 시작되고 PrepAnnounceSeconds 동안.
		//    이벤트가 아니라 경과 시간으로 판단한다. 알림 도중에 들어온 사람도 같은 화면을 봐야 한다
		if (Total - Remaining < PrepAnnounceSeconds)
		{
			// 남은 시간이 아니라 전체 길이를 쓴다. "이번 준비는 45초" 를 알리는 자리라
			// 44 · 43 으로 줄어들면 카운트다운으로 읽힌다
			ShowPrepCenter(LOCTEXT("PrepAnnounceLabel", "작전 준비"),
						   FText::Format(LOCTEXT("PrepAnnounceValue", "{0}초"), FText::AsNumber(FMath::CeilToInt(Total))),
						   PrepKeyAnnounce);
			SetPrepMini(INDEX_NONE);
			return;
		}

		// ③ 마지막 몇 초 — 다시 가운데 크게
		if (Seconds <= PrepFinalSeconds)
		{
			ShowPrepCenter(LOCTEXT("PrepFinalLabel", "작업 시작까지"), FText::AsNumber(Seconds), Seconds);
			SetPrepMini(INDEX_NONE);
			return;
		}

		// ② 그 사이 — 위쪽에 작게. 준비하는 동안 화면 가운데를 가리지 않는다
		HidePrepCenter();
		SetPrepMini(Seconds);
		return;
	}

	SetPrepMini(INDEX_NONE);

	// ④ 시작 — 본 작업 진입 직후 잠깐
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < StartBannerUntil)
	{
		ShowPrepCenter(FText::GetEmpty(), LOCTEXT("PrepStart", "시작!"), PrepKeyStart);
		return;
	}

	HidePrepCenter();
}

void UHeistHUDWidget::ShowPrepCenter(const FText& Label, const FText& Value, int32 Key)
{
	if (Key == LastPrepCenterKey)
	{
		return;
	}
	LastPrepCenterKey = Key;

	constexpr ESlateVisibility Shown = ESlateVisibility::SelfHitTestInvisible;

	if (Panel_PrepCenter) { Panel_PrepCenter->SetVisibility(Shown); }

	if (Txt_PrepCenterLabel)
	{
		// "시작!" 은 윗줄이 없다. 빈 글자를 남겨 두면 그 높이만큼 "시작!" 이 아래로 밀린다
		Txt_PrepCenterLabel->SetText(Label);
		Txt_PrepCenterLabel->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : Shown);
	}

	if (Txt_PrepCenterValue)
	{
		Txt_PrepCenterValue->SetText(Value);
		Txt_PrepCenterValue->SetVisibility(Shown);
	}

	if (PrepPop && !IsDesignTime())
	{
		PlayAnimation(PrepPop);
	}
}

void UHeistHUDWidget::HidePrepCenter()
{
	LastPrepCenterKey = INDEX_NONE;

	if (Panel_PrepCenter)    { Panel_PrepCenter->SetVisibility(ESlateVisibility::Collapsed); }
	if (Txt_PrepCenterLabel) { Txt_PrepCenterLabel->SetVisibility(ESlateVisibility::Collapsed); }
	if (Txt_PrepCenterValue) { Txt_PrepCenterValue->SetVisibility(ESlateVisibility::Collapsed); }
}

void UHeistHUDWidget::SetPrepMini(int32 Seconds)
{
	const ESlateVisibility Vis = (Seconds == INDEX_NONE) ? ESlateVisibility::Collapsed
														 : ESlateVisibility::SelfHitTestInvisible;
	if (Panel_PrepMini) { Panel_PrepMini->SetVisibility(Vis); }
	if (Txt_PrepMini)   { Txt_PrepMini->SetVisibility(Vis); }

	if (Seconds == INDEX_NONE)
	{
		LastPrepMiniSeconds = INDEX_NONE;
		return;
	}

	// 글자는 초가 바뀔 때만 새로 만든다. 이 함수는 0.1초마다 불린다
	if (Seconds != LastPrepMiniSeconds && Txt_PrepMini)
	{
		Txt_PrepMini->SetText(FormatClock(Seconds));
	}
	LastPrepMiniSeconds = Seconds;
}

// ──────────────────────────────────────────────────────────────
// 소지 슬롯 (기획서 8장 "소지 노획물")
// ──────────────────────────────────────────────────────────────

void UHeistHUDWidget::RefreshHeldSlot()
{
	AActor* Held = nullptr;

	// 자기 폰만 본다. 남이 뭘 들었는지는 이 슬롯의 관심사가 아니다.
	// 폰이 아직 없거나(스폰 전 · 관전) 다른 종류면 Held 는 null 이고 슬롯이 비는 것이 맞다
	if (const ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwningPlayerPawn()))
	{
		Held = Character->GetHeldActor();
	}

	const bool bWantFilled = IsValid(Held);

	// 들고 있던 노획물이 파괴되면 Held 도 LastHeldActor 도 null 이 된다.
	// 포인터만 비교하면 그때 다시 그릴 계기가 사라져 죽은 값이 화면에 남는다
	if (bHeldSlotDrawn && bHeldSlotFilled == bWantFilled && LastHeldActor.Get() == Held)
	{
		return;
	}

	bHeldSlotDrawn  = true;
	bHeldSlotFilled = bWantFilled;
	LastHeldActor   = Held;

	ApplyHeldSlot(Held);

	OnHeldChanged(Held);
}

void UHeistHUDWidget::ApplyHeldSlot(AActor* Held)
{
	FHeldDisplay Display;
	const bool bHasHeld = BuildHeldDisplay(Held, Display);

	if (Panel_Held)
	{
		Panel_Held->SetVisibility(bHasHeld ? ESlateVisibility::SelfHitTestInvisible
										   : ESlateVisibility::Collapsed);
	}
	else if (bHasHeld)
	{
		// 이름을 안 맞췄을 때 화면에는 "집었는데 아무것도 안 뜬다" 로만 드러난다.
		// 그 모습은 "아직 안 만든 것" 과 같아서 로그가 유일한 구분 수단이다
		UE_LOG(LogHeavyUI, Log,
			   TEXT("%s: WBP 에 Panel_Held 가 없어 소지 슬롯을 통째로 숨길 수 없다"), *GetName());
	}

	if (!bHasHeld)
	{
		// 숨겼어도 내용은 지운다 — Panel_Held 가 없는 WBP 에서는 이것이 유일한 방어다
		if (Txt_HeldName)   { Txt_HeldName->SetText(FText::GetEmpty()); }
		if (Txt_HeldInfo)   { Txt_HeldInfo->SetText(FText::GetEmpty()); }
		if (Bar_HeldWeight) { Bar_HeldWeight->SetPercent(0.f); }
		if (Img_Held)       { Img_Held->SetVisibility(ESlateVisibility::Collapsed); }

		UE_LOG(LogHeavyUI, Log, TEXT("%s: 소지 슬롯 비움"), *GetName());
		return;
	}

	if (Txt_HeldName)
	{
		Txt_HeldName->SetText(Display.Name);
	}

	if (Txt_HeldInfo)
	{
		// kg 은 정수로만 보여준다. 소수점이 흔들리면 시선이 그쪽으로 끌리고,
		// 무게는 어차피 '무거운가' 만 읽히면 되는 값이다
		const int32 RoundedKg = FMath::RoundToInt(Display.MassKg);

		Txt_HeldInfo->SetText(Display.bHasValue
			? FText::Format(LOCTEXT("HeldInfoWithValue", "{0}kg · ${1}"),
							FText::AsNumber(RoundedKg), FText::AsNumber(Display.Value))
			: FText::Format(LOCTEXT("HeldInfoMassOnly", "{0}kg"),
							FText::AsNumber(RoundedKg)));
	}

	if (Bar_HeldWeight)
	{
		// 분모는 게임플레이 판정이 아니라 표시 전용 값이다 (UUISettings::HeldWeightBarMaxKg 주석).
		// GetDefault<T>() 는 절대 null 이 아니므로 폴백 리터럴을 두지 않는다
		const float MaxKg = FMath::Max(1.f, UUISettings::Get()->HeldWeightBarMaxKg);
		Bar_HeldWeight->SetPercent(FMath::Clamp(Display.MassKg / MaxKg, 0.f, 1.f));
	}

	if (Img_Held)
	{
		UTexture2D* Icon = ResolveHeldIcon(Display.TypeTags);

		// 그림이 없으면 빈 흰 사각형이 남는다. 아이콘 자리가 아예 없는 편이 낫다
		Img_Held->SetVisibility(Icon ? ESlateVisibility::SelfHitTestInvisible
									 : ESlateVisibility::Collapsed);

		if (Icon)
		{
			Img_Held->SetBrushFromTexture(Icon, /*bMatchSize=*/false);
		}
	}

	// 무엇을 들었는지는 물리 파트가 이미 찍지만, 그것이 화면에 어떻게 나갔는지는 여기서만 안다.
	// 집고 놓을 때만 도는 경로라 반복 로그가 되지 않는다
	const FString ValuePart = Display.bHasValue
		? FString::Printf(TEXT(", $%d"), Display.Value)
		: FString();

	UE_LOG(LogHeavyUI, Log, TEXT("%s: 소지 슬롯 → \"%s\" (%.0fkg%s)"),
		   *GetName(), *Display.Name.ToString(), Display.MassKg, *ValuePart);
}

UTexture2D* UHeistHUDWidget::ResolveHeldIcon(const FGameplayTagContainer& TypeTags)
{
	const TSoftObjectPtr<UTexture2D> Soft = UUISettings::Get()->GetHeldSlotIcon(TypeTags);
	if (Soft.IsNull())
	{
		return nullptr;
	}

	// 캐시 키를 태그가 아니라 에셋 경로로 잡는다. 태그 여럿이 같은 그림을 가리킬 수 있다
	const FName Key(*Soft.ToSoftObjectPath().ToString());

	if (const TObjectPtr<UTexture2D>* Cached = HeldIconCache.Find(Key))
	{
		// null 이 들어 있으면 '전에 로드에 실패했다' 는 뜻이다. 다시 시도하지 않는다 —
		// 안 그러면 못 찾는 그림을 0.1초마다 다시 찾는다
		return Cached->Get();
	}

	UTexture2D* Loaded = Soft.LoadSynchronous();
	HeldIconCache.Add(Key, Loaded);

	if (!Loaded)
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: 소지 슬롯 아이콘을 로드하지 못했다 — %s "
					"(Project Settings → Game → UI → Held Slot)"),
			   *GetName(), *Key.ToString());
	}

	return Loaded;
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

// ──────────────────────────────────────────────────────────────
// 준비 카운트다운 흉내
//
// 쉬핑에서도 함수는 남는다(헤더에 선언돼 있다). 부르는 치트만 빠진다.
// ──────────────────────────────────────────────────────────────

void UHeistHUDWidget::StartDebugPrep(bool bFromStart)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Remaining = bFromStart ? UHeistSettings::Get()->PrepSeconds
									   : static_cast<float>(PrepFinalSeconds);

	DebugPrepEndTime   = World->GetTimeSeconds() + Remaining;
	bDebugPrepCounting = true;
	StartBannerUntil   = -1.f;

	// 같은 단계를 다시 틀어도 PrepPop 이 재생되도록 기록을 비운다
	HidePrepCenter();

	// 작업 레벨이 아니어도 돌아야 한다(L_UITest 같은 맵). 그래서 RefreshTimer 에 얹지 않고 따로 돈다
	World->GetTimerManager().SetTimer(
			DebugPrepHandle, this, &UHeistHUDWidget::StepDebugPrep, TimerTickInterval, true);

	UE_LOG(LogHeavyUI, Log, TEXT("%s: 준비 카운트다운 흉내 시작 — %.0f초 남음부터"), *GetName(), Remaining);

	StepDebugPrep();
}

void UHeistHUDWidget::StepDebugPrep()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		StopDebugPrep();
		return;
	}

	const float Now       = World->GetTimeSeconds();
	const float Remaining = DebugPrepEndTime - Now;

	if (Remaining > 0.f)
	{
		SetTimerVisible(false);
		RefreshPrepCountdown(/*bPrep=*/true, Remaining);
		return;
	}

	// 0 에 닿았다 — 진짜 페이즈 전환처럼 "시작!" 을 띄운다
	if (bDebugPrepCounting)
	{
		bDebugPrepCounting = false;
		StartBannerUntil   = DebugPrepEndTime + StartBannerSeconds;

		// 작업 레벨이면 원래 판을 되살린다. 아니면 판이 원래 숨어 있던 맵이다
		if (BoundState.Get())
		{
			LastShownSeconds = INDEX_NONE;
			RefreshTimer();
		}
	}

	RefreshPrepCountdown(/*bPrep=*/false, 0.f);

	if (Now >= StartBannerUntil)
	{
		StopDebugPrep();
	}
}

void UHeistHUDWidget::StopDebugPrep()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DebugPrepHandle);
	}
	DebugPrepHandle.Invalidate();

	DebugPrepEndTime   = -1.f;
	bDebugPrepCounting = false;
	StartBannerUntil   = -1.f;

	HidePrepCenter();
	SetPrepMini(INDEX_NONE);
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 치트
//
// 알림은 준비 시작 2초에만, 큰 숫자는 마지막 5초에만 나온다. 눈으로 맞추려면 매번 판을
// 새로 열고 기다려야 한다. hh.UI.Stamina 와 규칙을 맞춘다 — 쉬핑에서는 코드째 빠진다.
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

static void DebugPrepCommand(UWorld* World, bool bFromStart)
{
	int32 Applied = 0;
	for (int32 Index = GLiveHeistHUDs.Num() - 1; Index >= 0; --Index)
	{
		UHeistHUDWidget* Widget = GLiveHeistHUDs[Index].Get();
		if (!Widget)
		{
			GLiveHeistHUDs.RemoveAt(Index);
			continue;
		}

		// PIE 다중 창에서 남의 창 HUD 까지 건드리지 않도록 월드를 맞춘다
		if (World && Widget->GetWorld() != World)
		{
			continue;
		}

		Widget->StartDebugPrep(bFromStart);
		++Applied;
	}

	if (Applied == 0)
	{
		UE_LOG(LogHeavyUI, Warning, TEXT("화면에 떠 있는 HeistHUD 위젯이 없습니다. WBP_HUD 가 떠 있는지 확인할 것."));
	}
}

static FAutoConsoleCommandWithWorld GDebugPrepStartCommand(
	  TEXT("hh.UI.PrepStart"),
	  TEXT("hh.UI.PrepStart — 준비 카운트다운을 처음(작전 준비 45초)부터 화면에서만 흉내 낸다"),
	  FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { DebugPrepCommand(World, /*bFromStart=*/true); }),
	  ECVF_Cheat);

static FAutoConsoleCommandWithWorld GDebugPrepFinalCommand(
	  TEXT("hh.UI.PrepFinal"),
	  TEXT("hh.UI.PrepFinal — 준비 카운트다운의 마지막 5초(5 · 4 · 3 · 2 · 1 · 시작!)를 화면에서만 흉내 낸다"),
	  FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { DebugPrepCommand(World, /*bFromStart=*/false); }),
	  ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING

#undef LOCTEXT_NAMESPACE
