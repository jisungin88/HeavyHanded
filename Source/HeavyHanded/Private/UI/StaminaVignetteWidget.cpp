#include "UI/StaminaVignetteWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "Character/BaseAttributeSet.h"      // Stamina · MaxStamina 어트리뷰트 접근자
#include "Core/HeavyHandedGameplayTags.h"
#include "UI/HeavyUILog.h"

// ──────────────────────────────────────────────────────────────
// 화면에 떠 있는 인스턴스 목록
//
// 콘솔 명령은 static 함수라 위젯을 알 수 없다. 약참조로 모아 두고 치트가 찾는다.
// UDamageVignetteWidget 의 GLiveVignettes 와 같은 방식이다 — 쉬핑에서 치트는
// 통째로 빠지지만 등록 자체는 남겨 둔다. 등록/해제를 #if 로 갈라 두면
// NativeConstruct 가 빌드 구성마다 달라진다.
// ──────────────────────────────────────────────────────────────
namespace
{
	TArray<TWeakObjectPtr<UStaminaVignetteWidget>> GLiveStaminaVignettes;
}

// ──────────────────────────────────────────────────────────────
// 수명
// ──────────────────────────────────────────────────────────────

void UStaminaVignetteWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GLiveStaminaVignettes.RemoveAll([](const TWeakObjectPtr<UStaminaVignetteWidget>& Weak) { return !Weak.IsValid(); });
	GLiveStaminaVignettes.AddUnique(this);

	if (!Img_Fatigue)
	{
		UE_LOG(LogHeavyUI, Log,
			   TEXT("%s: WBP 에 Img_Fatigue 가 없어 화면에 아무것도 그리지 않는다"), *GetName());
	}

	// 시작은 항상 깨끗한 화면이다. WBP 에서 알파를 켜 둔 채 저장했더라도 여기서 되돌린다
	DisplayedIntensity = 0.f;
	TargetIntensity = 0.f;
	ApplyVisual();

	BindElapsed = 0.f;
	TryBind();
}

void UStaminaVignetteWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
		World->GetTimerManager().ClearTimer(InterpHandle);
	}

	Unbind();

	GLiveStaminaVignettes.Remove(this);

	Super::NativeDestruct();
}

void UStaminaVignetteWidget::TryBind()
{
	if (BoundASC)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ASC 는 폰이 아니라 PlayerState(APlayerSessionState)에 있다 — 체포→관전→복귀에도
	// 강화 수치가 살아남아야 하기 때문이다(규약 01 소유권 표). 폰에서 찾으면 영원히 못 찾는다.
	APlayerState* PS = GetOwningPlayerState();
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PS);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;

	if (!ASC)
	{
		BindElapsed += BindRetryInterval;

		// PlayerState 는 클라에 늦게 온다. 다만 GAS 가 없는 테스트 맵에서는 영영 안 오므로
		// 무한 재시도로 두면 아무 일도 안 하는 타이머가 계속 돈다
		if (BindElapsed >= BindGiveUpSeconds)
		{
			if (!bWarnedNoASC)
			{
				bWarnedNoASC = true;
				UE_LOG(LogHeavyUI, Warning,
					   TEXT("%s: %.0f초 동안 ASC 를 못 찾아 포기한다. 스태미나 이펙트가 뜨지 않는다 "
						    "(PlayerStateClass 가 APlayerSessionState 인지 확인할 것)"),
					   *GetName(), BindGiveUpSeconds);
			}
			World->GetTimerManager().ClearTimer(BindRetryHandle);
			return;
		}

		World->GetTimerManager().SetTimer(
				BindRetryHandle, this, &UStaminaVignetteWidget::TryBind, BindRetryInterval, false);
		return;
	}

	World->GetTimerManager().ClearTimer(BindRetryHandle);
	BoundASC = ASC;

	StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBaseAttributeSet::GetStaminaAttribute())
							  .AddUObject(this, &UStaminaVignetteWidget::HandleStaminaChanged);

	// MaxStamina 도 같이 본다. 분모가 바뀌는데 안 보면 같은 잔량이 다른 밝기로 그려진다
	MaxStaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBaseAttributeSet::GetMaxStaminaAttribute())
								 .AddUObject(this, &UStaminaVignetteWidget::HandleMaxStaminaChanged);

	ExhaustedTagHandle = ASC->RegisterGameplayTagEvent(HHTags::State_Exhausted, EGameplayTagEventType::NewOrRemoved)
							.AddUObject(this, &UStaminaVignetteWidget::HandleExhaustedTagChanged);

	// 구독 시점의 값으로 한 번 맞춘다. 안 하면 다음 변화까지 화면이 실제와 어긋난다
	StaminaValue    = ASC->GetNumericAttribute(UBaseAttributeSet::GetStaminaAttribute());
	MaxStaminaValue = ASC->GetNumericAttribute(UBaseAttributeSet::GetMaxStaminaAttribute());
	bExhausted      = ASC->HasMatchingGameplayTag(HHTags::State_Exhausted);

	UE_LOG(LogHeavyUI, Log, TEXT("%s: ASC 구독 완료 (스태미나 %.0f / %.0f%s)"),
		   *GetName(), StaminaValue, MaxStaminaValue, bExhausted ? TEXT(", 고갈") : TEXT(""));

	RefreshTarget();

	// 붙는 순간은 보간하지 않는다. 늦게 들어온 사람에게 화면이 서서히 어두워지면
	// 방금 자기가 달려서 숨이 찬 것처럼 보인다
	StopInterp();
	DisplayedIntensity = TargetIntensity;
	ApplyVisual();
	UpdatePulse();
}

void UStaminaVignetteWidget::Unbind()
{
	UAbilitySystemComponent* ASC = BoundASC;
	if (!ASC)
	{
		return;
	}

	ASC->GetGameplayAttributeValueChangeDelegate(UBaseAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(UBaseAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaChangedHandle);
	ASC->RegisterGameplayTagEvent(HHTags::State_Exhausted, EGameplayTagEventType::NewOrRemoved).Remove(ExhaustedTagHandle);

	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
	ExhaustedTagHandle.Reset();

	BoundASC = nullptr;
}

// ──────────────────────────────────────────────────────────────
// 구독 콜백
// ──────────────────────────────────────────────────────────────

void UStaminaVignetteWidget::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	StaminaValue = Data.NewValue;
	RefreshTarget();
}

void UStaminaVignetteWidget::HandleMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
	MaxStaminaValue = Data.NewValue;
	RefreshTarget();
}

void UStaminaVignetteWidget::HandleExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bNewExhausted = (NewCount > 0);
	if (bExhausted == bNewExhausted)
	{
		return;
	}
	bExhausted = bNewExhausted;

	// 상태가 바뀔 때만 찍는다. 태그 이벤트는 스택마다 오므로 조건 없이 로그하면 중복된다
	UE_LOG(LogHeavyUI, Log, TEXT("%s: 고갈 %s"), *GetName(), bExhausted ? TEXT("시작") : TEXT("해제"));

	RefreshTarget();

	// 치트로 화면을 강제한 동안에는 실제 태그가 바뀌어도 훅을 쏘지 않는다 —
	// 화면과 훅이 서로 다른 것을 말하게 된다
	if (!bDebugOverride)
	{
		OnExhaustedChanged(bExhausted);
	}
}

// ──────────────────────────────────────────────────────────────
// 계산
// ──────────────────────────────────────────────────────────────

float UStaminaVignetteWidget::GetEffectiveStamina01() const
{
	if (bDebugOverride)
	{
		return DebugStamina01;
	}

	// 분모가 0 이면 비율을 만들 수 없다. 안전한 쪽은 '가득 참'(= 이펙트 없음)이다 —
	// 반대로 두면 GAS 초기화가 늦은 한 프레임 동안 화면이 새까매진다
	if (MaxStaminaValue <= 0.f)
	{
		return 1.f;
	}

	return FMath::Clamp(StaminaValue / MaxStaminaValue, 0.f, 1.f);
}

bool UStaminaVignetteWidget::GetEffectiveExhausted() const
{
	return bDebugOverride ? bDebugExhausted : bExhausted;
}

float UStaminaVignetteWidget::GetStamina01() const
{
	return GetEffectiveStamina01();
}

bool UStaminaVignetteWidget::IsExhausted() const
{
	return GetEffectiveExhausted();
}

void UStaminaVignetteWidget::RefreshTarget()
{
	const float Stamina01 = GetEffectiveStamina01();

	if (GetEffectiveExhausted())
	{
		// 고갈 중에는 잔량을 보지 않는다. 회복은 태그와 무관하게 이미 돌고 있어서
		// 비례시키면 아직 스프린트가 막혀 있는데 화면이 먼저 걷힌다
		TargetIntensity = 1.f;
	}
	else if (LowStaminaThreshold01 <= 0.f)
	{
		TargetIntensity = 0.f;
	}
	else
	{
		TargetIntensity = FMath::Clamp((LowStaminaThreshold01 - Stamina01) / LowStaminaThreshold01, 0.f, 1.f);
	}

	StartInterp();
}

// ──────────────────────────────────────────────────────────────
// 보간
// ──────────────────────────────────────────────────────────────

void UStaminaVignetteWidget::StartInterp()
{
	if (IntensityInterpSpeed <= 0.f
		|| FMath::IsNearlyEqual(DisplayedIntensity, TargetIntensity, InterpSnapTolerance))
	{
		StopInterp();
		DisplayedIntensity = TargetIntensity;
		ApplyVisual();
		UpdatePulse();
		return;
	}

	if (InterpHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
				InterpHandle, this, &UStaminaVignetteWidget::StepInterp, InterpInterval, true);
	}
	else
	{
		DisplayedIntensity = TargetIntensity;
		ApplyVisual();
		UpdatePulse();
	}
}

void UStaminaVignetteWidget::StepInterp()
{
	DisplayedIntensity = FMath::FInterpTo(DisplayedIntensity, TargetIntensity, InterpInterval, IntensityInterpSpeed);

	// 남은 거리에 비례해 좁히는 방식이라 목표에 정확히 닿지 않는다.
	// 스냅하고 꺼 주지 않으면 타이머가 영원히 돈다
	if (FMath::IsNearlyEqual(DisplayedIntensity, TargetIntensity, InterpSnapTolerance))
	{
		DisplayedIntensity = TargetIntensity;
		StopInterp();
	}

	ApplyVisual();
	UpdatePulse();
}

void UStaminaVignetteWidget::StopInterp()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InterpHandle);
	}
	InterpHandle.Invalidate();
}

// ──────────────────────────────────────────────────────────────
// 표시
// ──────────────────────────────────────────────────────────────

void UStaminaVignetteWidget::ApplyVisual()
{
	if (Img_Fatigue)
	{
		if (DisplayedIntensity <= VisibleThreshold)
		{
			// 100~40% 구간은 화면에 아무것도 없다. 알파 0 짜리 풀스크린 이미지를
			// 남겨 두면 미션 내내 그려지기만 한다
			Img_Fatigue->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Img_Fatigue->SetVisibility(ESlateVisibility::HitTestInvisible);
			Img_Fatigue->SetRenderOpacity(DisplayedIntensity * MaxOpacity);
		}
	}

	OnIntensityUpdated(DisplayedIntensity);
}

void UStaminaVignetteWidget::UpdatePulse()
{
	EStaminaPulse Desired = EStaminaPulse::None;

	if (DisplayedIntensity > VisibleThreshold)
	{
		if (GetEffectiveExhausted())
		{
			Desired = EStaminaPulse::Hard;
		}
		else if (GetEffectiveStamina01() <= PulseStaminaThreshold01)
		{
			Desired = EStaminaPulse::Soft;
		}
	}

	if (Desired == ActivePulse)
	{
		return;
	}
	ActivePulse = Desired;

	if (!Breathe)
	{
		return;
	}

	if (Desired == EStaminaPulse::None)
	{
		StopAnimation(Breathe);
		return;
	}

	const float Speed = (Desired == EStaminaPulse::Hard) ? ExhaustedPulseSpeed : 1.f;

	// 루프 횟수 0 = 무한. 상태가 바뀔 때만 여기 오므로 매 스텝 재생이 끊기지 않는다
	PlayAnimation(Breathe, 0.f, 0, EUMGSequencePlayMode::Forward, Speed);
}

// ──────────────────────────────────────────────────────────────
// 치트 진입점
// ──────────────────────────────────────────────────────────────

void UStaminaVignetteWidget::SetDebugState(float NewStamina01, bool bNewExhausted)
{
	bDebugOverride = true;
	DebugStamina01 = FMath::Clamp(NewStamina01, 0.f, 1.f);
	bDebugExhausted = bNewExhausted;

	RefreshTarget();
}

void UStaminaVignetteWidget::ClearDebugState()
{
	if (!bDebugOverride)
	{
		return;
	}
	bDebugOverride = false;

	RefreshTarget();
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 치트
//
// 40% 아래를 눈으로 보려면 매번 숨이 찰 때까지 달려야 한다. 고갈 구간은 2.5초뿐이라
// 더하다. hh.UI.Damage 와 규칙을 맞춘다 — 쉬핑에서는 코드째 빠지고 ECVF_Cheat 를 단다.
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

static void StaminaVignetteCommand(const TArray<FString>& Args, UWorld* World)
{
	const bool bClear = (Args.Num() == 0) || Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase);

	const float Stamina01  = bClear ? 1.f : FMath::Clamp(FCString::Atof(*Args[0]), 0.f, 1.f);
	const bool  bExhausted = (Args.Num() > 1) && (FCString::Atoi(*Args[1]) != 0);

	int32 Applied = 0;
	for (int32 Index = GLiveStaminaVignettes.Num() - 1; Index >= 0; --Index)
	{
		UStaminaVignetteWidget* Widget = GLiveStaminaVignettes[Index].Get();
		if (!Widget)
		{
			GLiveStaminaVignettes.RemoveAt(Index);
			continue;
		}

		// 화면 분할 · PIE 다중 창에서 남의 창 위젯까지 건드리지 않도록 월드를 맞춘다
		if (World && Widget->GetWorld() != World)
		{
			continue;
		}

		if (bClear)
		{
			Widget->ClearDebugState();
		}
		else
		{
			Widget->SetDebugState(Stamina01, bExhausted);
		}
		++Applied;
	}

	if (Applied == 0)
	{
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("화면에 떠 있는 StaminaVignette 위젯이 없습니다. WBP 가 HUD 에 붙어 있는지 확인할 것."));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GStaminaVignetteCommand(
	  TEXT("hh.UI.Stamina"),
	  TEXT("hh.UI.Stamina <0~1> [고갈 0|1] — 스태미나 화면 이펙트를 강제한다. off 는 해제. 예: hh.UI.Stamina 0.2"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StaminaVignetteCommand),
	  ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING
