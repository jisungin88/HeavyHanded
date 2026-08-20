#include "UI/InteractPromptWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#include "Character/BaseCharacter.h"
#include "Core/GameStates/HeistGameState.h"
#include "Core/VanZone.h"
#include "Interfaces/Carryable.h"
#include "Loot/LootBase.h"
#include "UI/UISettings.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#endif

#define LOCTEXT_NAMESPACE "HeavyUI"

namespace
{
#if ENABLE_DRAW_DEBUG
	/**
	 * 시선 스윕을 눈으로 보게 한다.
	 *
	 * 프롬프트는 대상이 없으면 사라지는 것이 정상이라 화면만으로는 고장인지 알 수 없다.
	 * 무엇에 맞았는지 · 왜 숨겼는지를 띄워서 그 둘을 가른다.
	 */
	TAutoConsoleVariable<int32> CVarPromptDebug(
		TEXT("hh.UI.PromptDebug"),
		0,
		TEXT("상호작용 프롬프트의 시선 스윕을 그린다. 맞은 액터 이름과 숨김 사유를 화면에 띄운다."),
		ECVF_Cheat);

	/** 화면 메시지 키. 매 틱 새 줄이 쌓이지 않도록 같은 키로 덮어쓴다 */
	constexpr uint64 PromptDebugMessageKey = 0x48485549;   // "HHUI"

	void DrawPromptDebug(const UWorld* World, const FVector& Start, const FVector& End,
		const FHitResult& Hit, const FString& Reason)
	{
		if (!World || CVarPromptDebug.GetValueOnGameThread() <= 0)
		{
			return;
		}

		// 다음 갱신까지만 남긴다. RefreshInterval 보다 아주 조금 길게 잡아
		// 선이 깜빡이지 않게 한다
		const float Life = 0.12f;
		const bool bHit = Hit.bBlockingHit;

		DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End,
			bHit ? FColor::Green : FColor::Red, /*bPersistent=*/false, Life, 0, 1.f);

		if (bHit)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, 12.f, 12, FColor::Green, false, Life);
		}

		if (GEngine)
		{
			const FString HitName = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("(맞은 것 없음)");
			GEngine->AddOnScreenDebugMessage(PromptDebugMessageKey, Life + 0.05f, FColor::Yellow,
				FString::Printf(TEXT("[프롬프트] 대상: %s  |  %s"), *HitName, *Reason));
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

// ──────────────────────────────────────────────────────────────
// 수명
// ──────────────────────────────────────────────────────────────

void UInteractPromptWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// PreConstruct 는 디자이너에서도 실행된다. 토큰을 여기서 적용해야 편집 중에도
	// 실제 색 · 폰트가 보이고, UMG 에 색이 구워지는 것을 막는다
	if (Txt_Action)
	{
		Txt_Action->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Action->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	if (Txt_Detail)
	{
		Txt_Detail->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
		Txt_Detail->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextSecondary)));
	}
}

void UInteractPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 조준한 것이 없는 상태로 시작한다. 리썰 컴퍼니처럼 평소에는 화면 중앙이 비어 있다
	bPromptVisible = true;   // HidePrompt 가 조기 반환하지 않도록 뒤집어 둔다
	HidePrompt();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshHandle, this, &UInteractPromptWidget::RefreshFocus,
			RefreshInterval, /*bLoop=*/true);
	}
}

void UInteractPromptWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshHandle);
	}

	Super::NativeDestruct();
}

// ──────────────────────────────────────────────────────────────
// 조준 판정
// ──────────────────────────────────────────────────────────────

void UInteractPromptWidget::RefreshFocus()
{
	const APawn* Pawn = GetOwningPlayerPawn();
	const APlayerController* PC = GetOwningPlayer();
	UWorld* World = GetWorld();

	if (!World)
	{
		HidePrompt();
		return;
	}

	if (!Pawn || !PC)
	{
		// 메뉴 · 관전 · 리스폰 대기처럼 조종 중인 폰이 없는 순간이다. 정상 상태다
		DebugReason = TEXT("조종 중인 폰이나 컨트롤러가 없다");
		HidePrompt();

#if ENABLE_DRAW_DEBUG
		DrawPromptDebug(World, FVector::ZeroVector, FVector::ZeroVector, FHitResult(), DebugReason);
#endif
		return;
	}

	// ── ① 밴에 타고 있으면 시선과 무관하게 하차다 ──
	//
	// UGAB_Interact 도 시선 스윕보다 "먼저" 하차를 처리한다(VanZone.h 주석).
	// 순서를 맞춰야 탑승 중에 물건을 봤을 때 "집기" 가 뜨는 거짓말이 안 생긴다
	if (const AHeistGameState* GS = World->GetGameState<AHeistGameState>())
	{
		if (GS->IsBoarded(Pawn->GetPlayerState()))
		{
			DebugReason = TEXT("밴에 탑승 중 — 스윕 생략");
			ShowPrompt(LOCTEXT("PromptDisembark", "[E] 하차"), FText::GetEmpty());
			return;
		}
	}

	// ── ② 이미 들고 있으면 놓기 / 던지기 ──
	if (const ABaseCharacter* Char = Cast<ABaseCharacter>(Pawn))
	{
		if (AActor* Held = Char->GetHeldActor())
		{
			DebugReason = TEXT("이미 들고 있음 — 스윕 생략");
			ShowHoldingPrompt(Held);
			return;
		}
	}

	// ── ③ 시선 스윕 ──
	//
	// 카메라가 아니라 폰의 눈 위치에서 쏜다. 카메라를 기준으로 잡으면 3인칭 셋업에서
	// 스윕이 캐릭터 뒤에서 시작해 사거리를 캐릭터 몸통에 다 써버린다 — 자기 폰은
	// 무시하도록 되어 있어서 결국 아무것도 안 맞는다(2026-08-20 실제로 그랬다).
	//
	// 기획서 상호작용 거리 3m 도 "캐릭터로부터" 이지 "카메라로부터" 가 아니다.
	// 이렇게 두면 1인칭 · 3인칭 어느 쪽이어도 같은 결과가 나온다.
	const FVector Start = Pawn->GetPawnViewLocation();
	const FVector End   = Start + PC->GetControlRotation().Vector() * TraceRange;

	FCollisionQueryParams Params(TEXT("InteractPrompt"), /*bTraceComplex=*/false, Pawn);

	// ECC_Visibility 를 쓴다 — Interaction 채널은 정의만 돼 있고 죽어 있으며,
	// UGAB_Interact 도 이것으로 스윕한다. 여기서 채널이 갈리면 프롬프트와 실제 판정이 따로 논다
	FHitResult Hit;
	World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(TraceRadius), Params);

	DecidePrompt(Hit.GetActor(), Pawn);

#if ENABLE_DRAW_DEBUG
	DrawPromptDebug(World, Start, End, Hit, DebugReason);
#endif
}

void UInteractPromptWidget::DecidePrompt(AActor* Target, const APawn* Pawn)
{
	if (!IsValid(Target))
	{
		DebugReason = TEXT("스윕에 아무것도 안 맞음 (거리 밖이거나 허공)");
		HidePrompt();
		return;
	}

	// 밴 — 승차
	if (Target->IsA<AVanZone>())
	{
		DebugReason = TEXT("밴 — 승차");
		ShowPrompt(LOCTEXT("PromptBoard", "[E] 승차"), FText::GetEmpty());
		return;
	}

	const ICarryable* Carryable = Cast<ICarryable>(Target);
	if (!Carryable)
	{
		// 벽 · 바닥 · 캐릭터처럼 집을 수 없는 것에 맞았다. 대부분 여기다
		DebugReason = TEXT("맞았지만 ICarryable 이 아님");
		HidePrompt();
		return;
	}

	// 동료가 이미 잡고 있으면 합류다. 중량형의 "2인 필수" 가 여기서 드러난다
	if (const APawn* Primary = Carryable->GetPrimaryCarrier())
	{
		if (Primary != Pawn)
		{
			FText Who;
			if (const APlayerState* PS = Primary->GetPlayerState())
			{
				Who = FText::FromString(PS->GetPlayerName());
			}

			DebugReason = TEXT("동료가 잡고 있음 — 함께 들기");
			ShowPrompt(LOCTEXT("PromptCarryTogether", "[E] 함께 들기"), Who);
			return;
		}
	}

	// 가치는 파손 · 유출로 깎이므로 BaseValue 가 아니라 CurrentValue 를 본다(LootBase.h 주석)
	FText Detail;
	if (const ALootBase* Loot = Cast<ALootBase>(Target))
	{
		Detail = FText::Format(LOCTEXT("PromptValue", "${0}"),
			FText::AsNumber(Loot->GetCurrentValue()));
	}

	if (Carryable->GetRequiredCarriers() >= 2)
	{
		Detail = Detail.IsEmpty()
			? LOCTEXT("PromptNeedTwo", "2인 필요")
			: FText::Format(LOCTEXT("PromptValueNeedTwo", "{0} · 2인 필요"), Detail);
	}

	DebugReason = TEXT("집을 수 있음");
	ShowPrompt(LOCTEXT("PromptGrab", "[E] 집기"), Detail);
}

void UInteractPromptWidget::ShowHoldingPrompt(AActor* Held)
{
	FText Detail;
	if (const ALootBase* Loot = Cast<ALootBase>(Held))
	{
		Detail = FText::Format(LOCTEXT("PromptValue", "${0}"),
			FText::AsNumber(Loot->GetCurrentValue()));
	}

	// 던질 수 없는 물건에 던지기를 안내하지 않는다 — 중량형이 여기 해당한다
	const ICarryable* Carryable = Cast<ICarryable>(Held);
	const bool bThrowable = (Carryable != nullptr) && Carryable->CanBeThrown();

	ShowPrompt(bThrowable
			? LOCTEXT("PromptDropThrow", "[Q] 놓기    [좌클릭] 던지기")
			: LOCTEXT("PromptDrop", "[Q] 놓기"),
		Detail);
}

// ──────────────────────────────────────────────────────────────
// 표시
// ──────────────────────────────────────────────────────────────

void UInteractPromptWidget::ShowPrompt(const FText& Action, const FText& Detail)
{
	// 0.1초마다 도는 경로다. 같은 문구를 다시 꽂으면 Slate 가 매번 다시 그린다
	const bool bSameText = bPromptVisible
		&& LastAction.EqualTo(Action)
		&& LastDetail.EqualTo(Detail);

	if (bSameText)
	{
		return;
	}

	LastAction = Action;
	LastDetail = Detail;

	if (Txt_Action)
	{
		Txt_Action->SetText(Action);
	}

	if (Txt_Detail)
	{
		Txt_Detail->SetText(Detail);
		// 상세가 비어 있으면 줄만 남아 레이아웃이 흔들린다
		Txt_Detail->SetVisibility(Detail.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	// 프롬프트는 클릭 대상이 아니다 — 마우스 입력을 먹지 않도록 HitTestInvisible
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bPromptVisible = true;

	OnPromptShown(Action);
}

void UInteractPromptWidget::HidePrompt()
{
	if (!bPromptVisible)
	{
		return;
	}

	bPromptVisible = false;
	LastAction = FText::GetEmpty();
	LastDetail = FText::GetEmpty();

	SetVisibility(ESlateVisibility::Collapsed);

	OnPromptHidden();
}

#undef LOCTEXT_NAMESPACE
