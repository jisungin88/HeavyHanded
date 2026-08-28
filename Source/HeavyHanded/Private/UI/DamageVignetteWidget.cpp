#include "UI/DamageVignetteWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

#include "UI/HeavyUILog.h"

// ──────────────────────────────────────────────────────────────
// 치트가 위젯 인스턴스를 찾기 위한 등록부.
//
// 콘솔 명령은 static 함수라 위젯을 알 수 없다. 화면에 떠 있는 인스턴스를
// 여기 약참조로 모아 두고 치트가 그것을 찾는다. 약참조라 위젯이 사라져도
// 여기 남은 항목이 위젯의 수명을 붙잡지 않는다.
//
// 쉬핑 빌드에서는 치트가 통째로 빠지지만 등록 자체는 남겨 둔다 —
// 등록/해제를 #if 로 갈라 두면 NativeConstruct 가 빌드 구성마다 달라진다.
// ──────────────────────────────────────────────────────────────
namespace
{
	TArray<TWeakObjectPtr<UDamageVignetteWidget>> GLiveVignettes;
}

void UDamageVignetteWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GLiveVignettes.RemoveAll([](const TWeakObjectPtr<UDamageVignetteWidget>& Weak) { return !Weak.IsValid(); });
	GLiveVignettes.AddUnique(this);

	if (DamageTextures.Num() == 0)
	{
		UE_LOG(LogHeavyUI, Warning,
			TEXT("[%s] DamageTextures 가 비어 있습니다. 카운트는 오르지만 핏자국이 뜨지 않습니다. "
			     "/Game/BloodScreen/ScreenDamage_Blood/Textures 에서 5~10장을 담을 것."),
			*GetName());
	}

	if (GetMaxHitCount() == 0)
	{
		UE_LOG(LogHeavyUI, Warning,
			TEXT("[%s] Img_Blood_1~3 이 하나도 배치되지 않았습니다. 화면에 아무것도 그리지 않습니다."),
			*GetName());
	}

	// 시작은 항상 깨끗한 화면이다. WBP 에서 알파를 켜 둔 채 저장했더라도 여기서 되돌린다
	HitCount = 0;
	for (int32 Index = 0; Index < MaxSlots; ++Index)
	{
		if (UImage* BloodImage = GetSlotImage(Index))
		{
			BloodImage->SetRenderOpacity(0.f);
		}
	}
}

void UDamageVignetteWidget::NativeDestruct()
{
	// 위젯이 사라진 뒤에 타이머가 돌면 파괴된 이미지를 건드린다
	CancelFadeTimers();

	GLiveVignettes.Remove(this);

	Super::NativeDestruct();
}

int32 UDamageVignetteWidget::GetMaxHitCount() const
{
	// 중간이 비어 있으면(1 과 3 만 배치) 거기서 끊는다. 빈 슬롯을 건너뛰고 세면
	// 카운트와 화면의 핏자국 수가 어긋난다
	int32 Count = 0;
	while (Count < MaxSlots && GetSlotImage(Count) != nullptr)
	{
		++Count;
	}

	return Count;
}

float UDamageVignetteWidget::GetIntensity() const
{
	const int32 Max = GetMaxHitCount();

	return Max > 0 ? static_cast<float>(HitCount) / static_cast<float>(Max) : 0.f;
}

void UDamageVignetteWidget::SetHitCount(int32 NewCount)
{
	const int32 Clamped = FMath::Clamp(NewCount, 0, GetMaxHitCount());
	if (Clamped == HitCount)
	{
		return;
	}

	const int32 Previous = HitCount;
	HitCount = Clamped;

	if (HitCount == 0)
	{
		// 부활 · 리셋. 예약된 걷힘부터 취소해야 한다 —
		// 남겨 두면 ClearAll 연출이 끝난 뒤에 타이머가 다시 알파를 건드린다
		CancelFadeTimers();

		if (ClearAll)
		{
			PlayAnimation(ClearAll);
		}
		else
		{
			for (int32 Index = 0; Index < MaxSlots; ++Index)
			{
				if (UImage* BloodImage = GetSlotImage(Index))
				{
					BloodImage->SetRenderOpacity(0.f);
				}
			}
		}

		UE_LOG(LogHeavyUI, Verbose, TEXT("[%s] 피격 카운트 리셋"), *GetName());
		OnCleared();
		return;
	}

	// 늘어난 만큼만 새로 띄운다. 이미 떠 있는 핏자국은 건드리지 않는다 —
	// 연달아 맞으면 겹쳐 보이고, 각자 제 시간에 따로 걷힌다
	for (int32 Index = Previous; Index < HitCount; ++Index)
	{
		ShowSlot(Index);
	}

	// 줄었지만 0 은 아닌 경우(장비로 한 칸 회복 등)는 아직 설계에 없다.
	// 그때가 오면 여기서 초과분을 걷는다.

	UE_LOG(LogHeavyUI, Log, TEXT("[%s] 피격 %d/%d (강도 %.2f)"),
		*GetName(), HitCount, GetMaxHitCount(), GetIntensity());

	OnHitApplied(HitCount, GetIntensity());
}

void UDamageVignetteWidget::ShowSlot(int32 Index)
{
	UImage* BloodImage = GetSlotImage(Index);
	if (!BloodImage)
	{
		return;
	}

	if (UTexture2D* Mask = PickTexture())
	{
		// bMatchSize=false — 정렬로 화면 전체를 덮으므로 원본 크기를 따라가면 안 된다
		BloodImage->SetBrushFromTexture(Mask, false);
	}

	if (UWidgetAnimation* Anim = GetSlotAnim(Index))
	{
		PlayAnimation(Anim);
	}
	else
	{
		// 애니메이션이 없어도 보이기는 해야 한다. 연출은 나중에 붙이면 된다
		BloodImage->SetRenderOpacity(1.f);
	}

	// 마지막 단계는 다운이다. 여기서 화면이 깨끗해지면 일어난 것처럼 읽힌다
	const bool bIsFinal = (Index == GetMaxHitCount() - 1);
	if (bIsFinal && bKeepFinalHitVisible)
	{
		return;
	}

	if (HitFadeSeconds <= 0.f || !GetWorld())
	{
		return;
	}

	// 슬롯마다 따로 예약한다. 같은 슬롯에 다시 걸리는 경우는 없지만
	// (한 번 오른 카운트는 부활 전까지 내려가지 않는다) 방어적으로 덮어쓴다
	GetWorld()->GetTimerManager().SetTimer(
		FadeHandles[Index],
		FTimerDelegate::CreateUObject(this, &UDamageVignetteWidget::FadeSlot, Index),
		HitFadeSeconds,
		false);
}

void UDamageVignetteWidget::FadeSlot(int32 Index)
{
	UImage* BloodImage = GetSlotImage(Index);
	if (!BloodImage)
	{
		return;
	}

	// 등장 애니메이션을 거꾸로 돌린다. 사라지는 연출을 따로 만들면
	// 등장과 곡선이 어긋났을 때 한쪽만 고쳐져 티가 난다
	if (UWidgetAnimation* Anim = GetSlotAnim(Index))
	{
		PlayAnimationReverse(Anim, FadeSpeedScale);
	}
	else
	{
		BloodImage->SetRenderOpacity(0.f);
	}

	UE_LOG(LogHeavyUI, Verbose, TEXT("[%s] 핏자국 %d 걷힘 (카운트는 %d 유지)"),
		*GetName(), Index + 1, HitCount);

	OnHitFaded(Index);
}

void UDamageVignetteWidget::CancelFadeTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 Index = 0; Index < MaxSlots; ++Index)
	{
		World->GetTimerManager().ClearTimer(FadeHandles[Index]);
	}
}

UTexture2D* UDamageVignetteWidget::PickTexture()
{
	const int32 Num = DamageTextures.Num();
	if (Num == 0)
	{
		return nullptr;
	}

	int32 Index = FMath::RandRange(0, Num - 1);

	// 같은 그림이 연달아 나오면 "한 대 더 맞았다" 가 화면에서 안 읽힌다.
	// 한 장뿐이면 피할 방법이 없으므로 그대로 쓴다
	if (Num > 1 && Index == LastTextureIndex)
	{
		Index = (Index + 1) % Num;
	}

	LastTextureIndex = Index;

	return DamageTextures[Index];
}

UImage* UDamageVignetteWidget::GetSlotImage(int32 Index) const
{
	switch (Index)
	{
	case 0:  return Img_Blood_1;
	case 1:  return Img_Blood_2;
	case 2:  return Img_Blood_3;
	default: return nullptr;
	}
}

UWidgetAnimation* UDamageVignetteWidget::GetSlotAnim(int32 Index) const
{
	switch (Index)
	{
	case 0:  return Splat_1;
	case 1:  return Splat_2;
	case 2:  return Splat_3;
	default: return nullptr;
	}
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 치트
//
// 경비의 접촉 판정도 State.Downed 를 부여하는 쪽도 아직 없다. 이펙트를 눈으로
// 확인할 방법이 이것뿐이라 먼저 연다. 나중에 GameplayCue 가 SetHitCount() 를
// 부르게 되면 이 명령은 그대로 두고 트리거만 늘어난다.
//
// AlertComponent.cpp 의 hh.Alert.Set 과 규칙을 맞춘다 — 쉬핑에서는 코드째 빠지고,
// 개발 빌드에서도 ECVF_Cheat 를 단다.
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

static void DamageVignetteCommand(const TArray<FString>& Args, UWorld* World)
{
	const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;

	int32 Applied = 0;
	for (int32 Index = GLiveVignettes.Num() - 1; Index >= 0; --Index)
	{
		UDamageVignetteWidget* Widget = GLiveVignettes[Index].Get();
		if (!Widget)
		{
			GLiveVignettes.RemoveAt(Index);
			continue;
		}

		// 화면 분할 · PIE 다중 창에서 남의 창 위젯까지 건드리지 않도록 월드를 맞춘다
		if (World && Widget->GetWorld() != World)
		{
			continue;
		}

		Widget->SetHitCount(Count);
		++Applied;
	}

	if (Applied == 0)
	{
		UE_LOG(LogHeavyUI, Warning,
			TEXT("화면에 떠 있는 DamageVignette 위젯이 없습니다. WBP 가 HUD 에 붙어 있는지 확인할 것."));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GDamageVignetteCommand(
	  TEXT("hh.UI.Damage"),
	  TEXT("hh.UI.Damage <횟수> — 피격 화면 이펙트를 강제로 표시한다. 0 은 리셋. 예: hh.UI.Damage 2"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DamageVignetteCommand),
	  ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING
