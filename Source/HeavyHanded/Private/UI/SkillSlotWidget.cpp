#include "UI/SkillSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"                  // FGameplayEffectQuery — 남은 시간 조회

#include "UI/HeavyUILog.h"
#include "UI/UISettings.h"

namespace
{
	/**
	 * 태그에서 화면용 이름을 뽑는다. "Ability.Ghost.ShadowStep" → "ShadowStep".
	 *
	 * [임시] 표시 이름 데이터가 생기면 이 함수를 지운다.
	 */
	FText TagLeafToText(const FGameplayTag& Tag)
	{
		FString Leaf = Tag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), DotIndex))
		{
			Leaf.RightChopInline(DotIndex + 1);
		}
		return FText::FromString(Leaf);
	}

	/**
	 * 스킬 이름을 고른다. 역할 스킬 태그(Ability.<역할>.<스킬>)가 있으면 그것,
	 * 없으면 쿨다운 태그 마지막 마디를 쓴다 — 어빌리티 태그를 안 붙인 스킬도 이름은 나와야 한다.
	 */
	FText MakeSkillName(const UGameplayAbility& Ability, const FGameplayTagContainer& InCooldownTags)
	{
		static const FGameplayTag AbilityRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability"), false);

		for (const FGameplayTag& Tag : Ability.AbilityTags)
		{
			if (AbilityRoot.IsValid() && Tag.MatchesTag(AbilityRoot))
			{
				return TagLeafToText(Tag);
			}
		}

		if (InCooldownTags.Num() > 0)
		{
			return TagLeafToText(InCooldownTags.GetByIndex(0));
		}

		return FText::FromString(Ability.GetClass()->GetName());
	}
}

// ──────────────────────────────────────────────────────────────
// 수명
// ──────────────────────────────────────────────────────────────

void USkillSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// PreConstruct 는 디자이너에서도 돈다. 토큰을 여기서 칠해야 편집 중에도 실제 색 · 폰트가 보인다
	if (Txt_Cooldown)
	{
		Txt_Cooldown->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Cooldown->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_SkillName)
	{
		Txt_SkillName->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
		Txt_SkillName->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextSecondary)));
	}

	if (Img_CooldownDim)
	{
		FLinearColor Dim = UUISettings::GetUIColor(EUIColorToken::BgBase);
		Dim.A = DimOpacity;
		Img_CooldownDim->SetColorAndOpacity(Dim);

		// 기준점을 아래 가운데로 둔다. 세로 스케일을 줄이면 위에서부터 걷혀 아래로 내려가는 것처럼 보인다.
		// WBP 에서 피벗을 따로 맞추지 않아도 되게 여기서 고정한다
		Img_CooldownDim->SetRenderTransformPivot(FVector2D(0.5, 1.0));
	}
}

void USkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 찾기 전에는 빈 칸이다. 디자이너에서 덮개를 켜 둔 채 저장했더라도 여기서 되돌린다
	bOnCooldown = false;
	SkillName = FText::GetEmpty();
	ApplyVisual();

	if (Txt_SkillName)
	{
		Txt_SkillName->SetText(SkillName);
	}

	// 스킬을 찾기 전에는 그림도 없다. 디자이너에서 꽂아 둔 미리보기 그림이 남지 않게 숨긴다
	if (Img_SkillIcon)
	{
		Img_SkillIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	BindElapsed = 0.f;
	TryBind();
}

void USkillSlotWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryHandle);
		World->GetTimerManager().ClearTimer(CooldownTickHandle);
	}

	Unbind();

	Super::NativeDestruct();
}

// ──────────────────────────────────────────────────────────────
// 바인딩
// ──────────────────────────────────────────────────────────────

void USkillSlotWidget::TryBind()
{
	if (IsSkillBound())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	BindElapsed += BindRetryInterval;

	// ASC 는 폰이 아니라 PlayerState(APlayerSessionState)에 있다 (규약 01 소유권 표)
	APlayerState* PS = GetOwningPlayerState();
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PS);
	UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;

	if (!ASC)
	{
		if (BindElapsed >= BindGiveUpSeconds)
		{
			if (!bWarnedNoASC)
			{
				bWarnedNoASC = true;
				UE_LOG(LogHeavyUI, Warning,
					   TEXT("%s: %.0f초 동안 ASC 를 못 찾아 포기한다. 스킬 칸이 비어 있게 된다 "
						    "(PlayerStateClass 가 APlayerSessionState 인지 확인할 것)"),
					   *GetName(), BindGiveUpSeconds);
			}
			World->GetTimerManager().ClearTimer(BindRetryHandle);
			return;
		}

		World->GetTimerManager().SetTimer(
				BindRetryHandle, this, &USkillSlotWidget::TryBind, BindRetryInterval, false);
		return;
	}

	const UGameplayAbility* Ability = FindCooldownAbility(*ASC);
	if (!Ability)
	{
		// 폰이 아직 스폰 · 빙의 전이면 어빌리티가 부여되지 않았다.
		// 포기하지 않고 계속 찾는다 (헤더 BindGiveUpSeconds 주석)
		if (BindElapsed >= BindGiveUpSeconds && !bWarnedNoSkill)
		{
			bWarnedNoSkill = true;
			UE_LOG(LogHeavyUI, Warning,
				   TEXT("%s: ASC 는 있는데 %.0f초 동안 쿨다운 있는 스킬이 없다. 계속 찾는다 "
					    "(캐릭터 BP 의 AbilityInputBindings · 스킬 GA 의 Cooldown Gameplay Effect Class 확인)"),
				   *GetName(), BindGiveUpSeconds);
		}

		World->GetTimerManager().SetTimer(
				BindRetryHandle, this, &USkillSlotWidget::TryBind, BindRetryInterval, false);
		return;
	}

	World->GetTimerManager().ClearTimer(BindRetryHandle);

	BoundASC = ASC;
	BoundAbility = Ability;
	CooldownTags = *Ability->GetCooldownTags();   // FindCooldownAbility 가 비어 있지 않음을 보장한다
	SkillName = MakeSkillName(*Ability, CooldownTags);
	ApplyIcon();

	// 이름은 바인딩 때 한 번만 쓴다. ApplyVisual 은 쿨다운 중 초당 60번 돌아서 거기 두지 않는다
	if (Txt_SkillName)
	{
		Txt_SkillName->SetText(SkillName);
	}

	for (const FGameplayTag& Tag : CooldownTags)
	{
		FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
									.AddUObject(this, &USkillSlotWidget::HandleCooldownTagChanged);
		CooldownTagHandles.Emplace(Tag, Handle);
	}

	UE_LOG(LogHeavyUI, Log, TEXT("%s: 스킬 칸 바인딩 — %s (%s, 쿨다운 태그 %s)"),
		   *GetName(), *SkillName.ToString(), *Ability->GetClass()->GetName(), *CooldownTags.ToStringSimple());

	// 붙는 순간의 상태로 맞춘다. 이미 쿨다운 중이어도 시작 훅은 쏘지 않는다 (헤더 OnCooldownStarted 주석)
	RefreshCooldown(false);
	OnSkillBound(SkillName);
}

const UGameplayAbility* USkillSlotWidget::FindCooldownAbility(const UAbilitySystemComponent& ASC)
{
	for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = Spec.Ability;
		if (!Ability)
		{
			continue;
		}

		const FGameplayTagContainer* Tags = Ability->GetCooldownTags();
		if (Tags && Tags->Num() > 0)
		{
			return Ability;
		}
	}
	return nullptr;
}

void USkillSlotWidget::Unbind()
{
	if (UAbilitySystemComponent* ASC = BoundASC)
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : CooldownTagHandles)
		{
			ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
		}
	}

	CooldownTagHandles.Reset();
	CooldownTags.Reset();
	BoundAbility.Reset();
	BoundASC = nullptr;
}

// ──────────────────────────────────────────────────────────────
// 쿨다운
// ──────────────────────────────────────────────────────────────

void USkillSlotWidget::HandleCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	RefreshCooldown(true);
}

void USkillSlotWidget::RefreshCooldown(bool bFireHooks)
{
	UAbilitySystemComponent* ASC = BoundASC;
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		return;
	}

	const bool bNewOnCooldown = ASC->HasAnyMatchingGameplayTags(CooldownTags);

	if (bNewOnCooldown)
	{
		ReadRemaining();
	}

	if (bNewOnCooldown == bOnCooldown)
	{
		// 태그 이벤트는 스택마다 온다. 상태가 그대로면 남은 시간만 갱신된 것이다
		ApplyVisual();
		return;
	}

	bOnCooldown = bNewOnCooldown;
	LastShownSeconds = INDEX_NONE;

	if (bOnCooldown)
	{
		World->GetTimerManager().SetTimer(
				CooldownTickHandle, this, &USkillSlotWidget::TickCooldown, CooldownTickInterval, true);
	}
	else
	{
		World->GetTimerManager().ClearTimer(CooldownTickHandle);
		RemainingSeconds = 0.f;
		DurationSeconds = 0.f;
	}

	// 상태가 바뀔 때만 찍는다
	UE_LOG(LogHeavyUI, Log, TEXT("%s: %s 쿨다운 %s (%.1f / %.1f초)"),
		   *GetName(), *SkillName.ToString(), bOnCooldown ? TEXT("시작") : TEXT("끝"),
		   RemainingSeconds, DurationSeconds);

	ApplyVisual();

	if (bFireHooks)
	{
		if (bOnCooldown)
		{
			OnCooldownStarted(DurationSeconds);
		}
		else
		{
			OnCooldownEnded();
		}
	}
}

void USkillSlotWidget::ReadRemaining()
{
	UAbilitySystemComponent* ASC = BoundASC;
	if (!ASC)
	{
		return;
	}

	// 같은 태그를 주는 GE 가 여럿이면 가장 늦게 끝나는 것을 본다
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	float BestRemaining = 0.f;
	float BestDuration = 0.f;
	for (const TPair<float, float>& Pair : ASC->GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (Pair.Key > BestRemaining)
		{
			BestRemaining = Pair.Key;
			BestDuration = Pair.Value;
		}
	}

	RemainingSeconds = FMath::Max(BestRemaining, 0.f);
	DurationSeconds = FMath::Max(BestDuration, 0.f);
}

void USkillSlotWidget::TickCooldown()
{
	if (!BoundASC)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CooldownTickHandle);
		}
		return;
	}

	ReadRemaining();
	ApplyVisual();
	OnCooldownUpdated(RemainingSeconds, GetCooldownProgress01());
}

float USkillSlotWidget::GetCooldownProgress01() const
{
	if (!bOnCooldown || DurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	return FMath::Clamp(1.f - RemainingSeconds / DurationSeconds, 0.f, 1.f);
}

float USkillSlotWidget::GetCooldownRemaining01() const
{
	if (!bOnCooldown)
	{
		return 0.f;
	}

	// 태그는 붙었는데 GE 시간을 아직 못 읽었으면 꽉 찬 것으로 본다.
	// 1 - Progress 로 쓰면 이때 Progress 가 1 이라 덮개가 안 보인 채 쿨다운이 돈다
	if (DurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	return FMath::Clamp(RemainingSeconds / DurationSeconds, 0.f, 1.f);
}

// ──────────────────────────────────────────────────────────────
// 표시
// ──────────────────────────────────────────────────────────────

void USkillSlotWidget::ApplyVisual()
{
	if (Img_CooldownDim)
	{
		// [순서] 크기를 먼저 맞추고 보이게 한다. 거꾸로 하면 지난 쿨다운 끝의 얇은 덮개가
		//   한 프레임 보였다가 꽉 차면서 깜빡인다
		Img_CooldownDim->SetRenderScale(FVector2D(1., bOnCooldown ? GetCooldownRemaining01() : 1.));

		// 덮개가 클릭을 먹으면 안 된다. WBP 설정과 상관없이 여기서 HitTestInvisible 로 고정한다
		Img_CooldownDim->SetVisibility(bOnCooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!Txt_Cooldown)
	{
		return;
	}

	if (!bOnCooldown)
	{
		Txt_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
		LastShownSeconds = INDEX_NONE;
		return;
	}

	Txt_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 올림이다 — 0.4초 남았을 때 "0" 이 뜨면 이미 쓸 수 있는 것처럼 읽힌다
	const int32 Seconds = FMath::CeilToInt(RemainingSeconds);
	if (Seconds != LastShownSeconds)
	{
		LastShownSeconds = Seconds;
		Txt_Cooldown->SetText(FText::AsNumber(Seconds));
	}
}

void USkillSlotWidget::ApplyIcon()
{
	if (!Img_SkillIcon)
	{
		return;
	}

	const TSoftObjectPtr<UTexture2D> Soft = UUISettings::Get()->GetSkillIcon(CooldownTags);
	UTexture2D* Loaded = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();

	if (!Loaded)
	{
		// 표에 행이 없는 것과 경로가 깨진 것을 구분해서 남긴다 — 고칠 곳이 다르다
		if (Soft.IsNull())
		{
			UE_LOG(LogHeavyUI, Warning,
				   TEXT("%s: %s 아이콘이 표에 없다 (Project Settings → Game → UI → Skill Slot → Skill Icons 에 %s 행 추가)"),
				   *GetName(), *SkillName.ToString(), *CooldownTags.ToStringSimple());
		}
		else
		{
			UE_LOG(LogHeavyUI, Warning, TEXT("%s: %s 아이콘을 로드하지 못했다 — %s"),
				   *GetName(), *SkillName.ToString(), *Soft.ToSoftObjectPath().ToString());
		}

		Img_SkillIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 크기는 WBP 배치를 따른다. 텍스처 원본 크기에 맞추면 이미지마다 칸 크기가 달라진다
	Img_SkillIcon->SetBrushFromTexture(Loaded, false);
	Img_SkillIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
}
