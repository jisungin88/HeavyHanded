#include "UI/HeistResultWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistPhase.h"
#include "Core/PlayerControllers/HeistPlayerController.h"
#include "UI/HeavyUILog.h"
#include "UI/HeavyUIText.h"
#include "UI/UISettings.h"
#include "Components/PanelWidget.h"
#include "UI/ResultRowWidget.h"

#define LOCTEXT_NAMESPACE "HeavyUI"

namespace
{
	/**
	 * 등급별 큰 제목.
	 *
	 * 기획서 2장 승패 조건이 세 단계인데 시안은 "탈출 성공!" 하나만 그려져 있다.
	 * 부분 성공을 성공으로 묶어 버리면 목표를 채운 판과 못 채운 판이 같은 화면이 되고,
	 * 다음 판에 무엇을 고쳐야 하는지가 사라진다.
	 */
	FText OutcomeToTitle(EHeistOutcome Outcome)
	{
		switch (Outcome)
		{
		case EHeistOutcome::Success: return LOCTEXT("ResultSuccess", "탈출 성공!");
		case EHeistOutcome::Partial: return LOCTEXT("ResultPartial", "부분 성공");
		default:                     return LOCTEXT("ResultFailure", "작전 실패");
		}
	}

	/**
	 * 부제. 등급과 '왜 끝났는가' 를 같이 본다.
	 *
	 * 같은 성공이라도 경보를 뚫고 나온 판과 시간에 맞춰 나온 판은 다른 이야기다.
	 * 사유(EHeistPhaseReason)가 복제되는 것이 이런 데 쓰라고 있는 값이다.
	 */
	FText MakeSubtitle(EHeistOutcome Outcome, EHeistPhaseReason Reason)
	{
		const bool bAlarm = (Reason == EHeistPhaseReason::Alarm);

		switch (Outcome)
		{
		case EHeistOutcome::Success:
			return bAlarm
				? LOCTEXT("ResultSubSuccessAlarm", "경보를 뚫고 목표를 채웠다")
				: LOCTEXT("ResultSubSuccess",      "수집한 전리품을 챙기고 돌아왔다");

		case EHeistOutcome::Partial:
			return bAlarm
				? LOCTEXT("ResultSubPartialAlarm", "들켰다. 챙길 수 있는 만큼만 챙겼다")
				: LOCTEXT("ResultSubPartial",      "목표 금액을 채우지 못했다");

		default:
			return LOCTEXT("ResultSubFailure", "아무도 돌아오지 못했다");
		}
	}

	/** 적재 목록 한 줄을 만들기 위한 묶음. 같은 노획물 여러 개를 한 줄로 합친다 */
	struct FLootLine
	{
		FText Name;
		int32 Count = 0;
		int32 Value = 0;
		int32 BaseValue = 0;
	};

	/** 개인 기여도 한 줄 */
	struct FPlayerLine
	{
		FString Name;
		int32 Value = 0;
		FText Status;
	};

	/** 적재 한 줄의 왼쪽 문구. 여러 개면 "도자기 세트 x5" 로 묶어 보인다 */
	FText LootLineLabel(const FLootLine& Line)
	{
		return (Line.Count > 1)
			? FText::Format(LOCTEXT("ResultLootNameMany", "{0} x{1}"),
							Line.Name, FText::AsNumber(Line.Count))
			: Line.Name;
	}

	/**
	 * 적재 목록을 묶어 정렬한다.
	 *
	 * [여기 한 곳에만 둔다] 여러 줄 텍스트와 행 위젯이 같은 목록을 그린다.
	 *   각자 집계하면 둘이 다른 순서 · 다른 묶음을 보여줄 수 있고,
	 *   그때 어느 쪽이 맞는지 판단할 근거가 없다.
	 */
	void CollectLootLines(const AHeistGameState& GS, TArray<FLootLine>& Out)
	{
		// 같은 종류를 한 줄로 묶는다. 도자기 세트를 다섯 번 실었으면
		// 다섯 줄이 아니라 "도자기 세트 x5" 한 줄이어야 읽힌다
		for (const FHeistLoadEntry& Entry : GS.GetLoadedEntries())
		{
			const FText Name = HeavyUIText::LootName(Entry.LootClass);

			FLootLine* Existing = Out.FindByPredicate(
				[&Name](const FLootLine& Line) { return Line.Name.EqualTo(Name); });

			if (Existing)
			{
				++Existing->Count;
				Existing->Value     += Entry.Value;
				Existing->BaseValue += Entry.BaseValue;
			}
			else
			{
				Out.Add({ Name, 1, Entry.Value, Entry.BaseValue });
			}
		}

		// 비싼 것부터. 무엇을 잘 챙겼는지가 위에 와야 한다
		Out.Sort([](const FLootLine& A, const FLootLine& B) { return A.Value > B.Value; });
	}

	/** 플레이어 목록을 모아 정렬한다. 많이 벌어온 사람부터 */
	void CollectPlayerLines(const AHeistGameState& GS, TArray<FPlayerLine>& Out)
	{
		for (APlayerState* Player : GS.PlayerArray)
		{
			// 관전자와 나간 사람을 세는 기준은 GameState 가 이미 갖고 있다.
			// 여기서 따로 세면 결과 화면과 판정이 다른 인원을 말하게 된다
			if (!AHeistGameState::IsCountedPlayer(Player))
			{
				continue;
			}

			FText Status;
			if (GS.IsArrested(Player))
			{
				Status = LOCTEXT("ResultStatusArrested", "체포");
			}
			else if (GS.IsBoarded(Player))
			{
				Status = LOCTEXT("ResultStatusEscaped", "탈출");
			}
			else
			{
				// 결과 페이즈에서는 체포 확정이 이미 끝났으므로 여기 오는 사람은 거의 없다.
				// 그래도 빈칸으로 두지 않는다 — 빈칸은 버그와 구별되지 않는다
				Status = LOCTEXT("ResultStatusUnknown", "-");
			}

			Out.Add({ Player->GetPlayerName(), GS.GetContributionOf(Player), Status });
		}

		Out.Sort([](const FPlayerLine& A, const FPlayerLine& B) { return A.Value > B.Value; });
	}
}

// ──────────────────────────────────────────────────────────────
// 수명
// ──────────────────────────────────────────────────────────────

void UHeistResultWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// PreConstruct 는 디자이너에서도 실행된다. 토큰을 여기서 적용해야 편집 중에도
	// 실제 색 · 폰트가 보이고, UMG 에 색이 구워지는 것을 막는다 (UISettings.h 주석)
	if (Txt_Outcome)
	{
		Txt_Outcome->SetFont(UUISettings::GetUIFont(EUIFontToken::Timer));
		Txt_Outcome->SetColorAndOpacity(FSlateColor(GetOutcomeColor()));
	}

	if (Txt_Subtitle)
	{
		Txt_Subtitle->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
		Txt_Subtitle->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextSecondary)));
	}

	// 수치 셋은 같은 급으로 읽혀야 한다 — 하나만 크면 그것부터 보게 된다
	for (UTextBlock* Value : { Txt_Elapsed.Get(), Txt_Money.Get(), Txt_LootCount.Get() })
	{
		if (Value)
		{
			Value->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
			Value->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::Money)));
		}
	}

	// 최다 소음 유발자 이름은 시안에서 크게 나온다 — 라벨 급으로 두면 안 읽힌다
	if (Txt_Noisiest)
	{
		Txt_Noisiest->SetFont(UUISettings::GetUIFont(EUIFontToken::Value));
		Txt_Noisiest->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
	}

	// 부연 · 달성률은 보조 정보다
	for (UTextBlock* Sub : { Txt_NoisiestDetail.Get(), Txt_TargetRatio.Get() })
	{
		if (Sub)
		{
			Sub->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
			Sub->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextSecondary)));
		}
	}

	for (UTextBlock* Label : { Txt_LootList.Get(), Txt_PlayerList.Get(), Txt_Confirm.Get() })
	{
		if (Label)
		{
			Label->SetFont(UUISettings::GetUIFont(EUIFontToken::Label));
			Label->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(EUIColorToken::TextPrimary)));
		}
	}
}

void UHeistResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// BindWidgetOptional 은 이름이 틀려도 컴파일이 통과한다. 그때 드러나는 모습은
	// "그 칸만 비어 있는 화면" 이라 눈으로는 오타인지 미완성인지 알 수 없다
	const TCHAR* MissingNames[] =
	{
		Txt_Subtitle  ? nullptr : TEXT("Txt_Subtitle"),
		Txt_Elapsed   ? nullptr : TEXT("Txt_Elapsed"),
		Txt_Money     ? nullptr : TEXT("Txt_Money"),
		Txt_LootCount ? nullptr : TEXT("Txt_LootCount"),
		Txt_Noisiest  ? nullptr : TEXT("Txt_Noisiest"),
		Txt_LootList  ? nullptr : TEXT("Txt_LootList"),
		Txt_PlayerList? nullptr : TEXT("Txt_PlayerList"),
		Txt_Confirm   ? nullptr : TEXT("Txt_Confirm"),
		Btn_Confirm   ? nullptr : TEXT("Btn_Confirm"),
	};

	for (const TCHAR* Missing : MissingNames)
	{
		if (Missing)
		{
			UE_LOG(LogHeavyUI, Log, TEXT("%s: WBP 에 %s 가 없어 그 칸을 채우지 않는다"),
				   *GetName(), Missing);
		}
	}

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.AddDynamic(this, &UHeistResultWidget::HandleConfirmClicked);
	}

	BindToHeistGameState();

	if (Intro && !IsDesignTime())
	{
		PlayAnimation(Intro);
	}
}

void UHeistResultWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownHandle);

		if (GameStateSetHandle.IsValid())
		{
			World->GameStateSetEvent.Remove(GameStateSetHandle);
			GameStateSetHandle.Reset();
		}
	}

	if (Btn_Confirm)
	{
		Btn_Confirm->OnClicked.RemoveDynamic(this, &UHeistResultWidget::HandleConfirmClicked);
	}

	// 구독을 안 풀면 위젯이 사라진 뒤에도 델리게이트에 남는다
	if (AHeistGameState* GS = BoundState.Get())
	{
		GS->OnResultConfirmChanged.RemoveDynamic(this, &UHeistResultWidget::HandleConfirmCountChanged);
	}
	BoundState = nullptr;

	Super::NativeDestruct();
}

// ──────────────────────────────────────────────────────────────
// 바인딩
// ──────────────────────────────────────────────────────────────

void UHeistResultWidget::BindToHeistGameState()
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

	AHeistGameState* GS = World->GetGameState<AHeistGameState>();
	if (!GS)
	{
		// 이 위젯은 AHeavyHUD 가 결과 페이즈에서 만들므로 GameState 는 이미 와 있어야 한다.
		// 그래도 대비하는 이유는, 그 전제가 깨지면 화면 전체가 빈 채로 뜨기 때문이다
		if (!GameStateSetHandle.IsValid())
		{
			GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &UHeistResultWidget::HandleGameStateSet);
		}
		return;
	}

	BoundState = GS;

	GS->OnResultConfirmChanged.AddDynamic(this, &UHeistResultWidget::HandleConfirmCountChanged);

	PopulateResult();

	// 확인 인원은 지금 값으로 한 번 맞춘다. 늦게 뜬 화면이 "0/4" 로 시작하면 안 된다
	HandleConfirmCountChanged(GS->GetResultConfirmedNum(), GS->PlayerArray.Num());

	World->GetTimerManager().SetTimer(
			CountdownHandle, this, &UHeistResultWidget::RefreshCountdown, CountdownInterval, true);

	RefreshCountdown();
}

void UHeistResultWidget::HandleGameStateSet(AGameStateBase* NewGameState)
{
	if (!Cast<AHeistGameState>(NewGameState))
	{
		return;
	}

	BindToHeistGameState();
}

// ──────────────────────────────────────────────────────────────
// 한 번만 그리는 것들
// ──────────────────────────────────────────────────────────────

void UHeistResultWidget::PopulateResult()
{
	const AHeistGameState* GS = BoundState.Get();
	if (!GS)
	{
		return;
	}

	const EHeistOutcome Outcome = GS->GetOutcome();

	if (Txt_Outcome)
	{
		Txt_Outcome->SetText(OutcomeToTitle(Outcome));
		Txt_Outcome->SetColorAndOpacity(FSlateColor(GetOutcomeColor()));
	}

	if (Txt_Subtitle)
	{
		Txt_Subtitle->SetText(MakeSubtitle(Outcome, GS->GetPhaseReason()));
	}

	if (Txt_Elapsed)
	{
		Txt_Elapsed->SetText(HeavyUIText::Duration(GS->GetElapsedSeconds()));
	}

	if (Txt_Money)
	{
		Txt_Money->SetText(FText::Format(LOCTEXT("ResultMoney", "{0} / {1}"),
										 HeavyUIText::Money(GS->GetLoadedValue()),
										 HeavyUIText::Money(GS->GetTargetValue())));

		// 목표를 채웠으면 금색. HUD 의 목표 금액과 같은 규칙이라 두 화면이 같은 말을 한다
		const EUIColorToken Token = GS->IsTargetReached() ? EUIColorToken::Gold : EUIColorToken::Money;
		Txt_Money->SetColorAndOpacity(FSlateColor(UUISettings::GetUIColor(Token)));
	}

	if (Txt_LootCount)
	{
		Txt_LootCount->SetText(FText::Format(LOCTEXT("ResultLootCount", "{0}개"),
											 FText::AsNumber(GS->GetLoadedEntries().Num())));
	}

	if (Txt_Noisiest)
	{
		// 기획서 8장이 "반드시 넣는다" 고 못박은 항목이다.
		// 값의 주인은 UAlertComponent 지만 GameState 가 받아 넘겨 준다
		float Contribution = 0.f;
		const APlayerState* Noisiest = GS->GetNoisiestPlayer(Contribution);

		// 시안은 이름만 크게 두고 라벨("최다 소음 유발자")은 WBP 의 고정 문구다.
		// 여기서 라벨까지 넣으면 글자 크기를 나눌 수 없다
		Txt_Noisiest->SetText(Noisiest
			? FText::FromString(Noisiest->GetPlayerName())
			: LOCTEXT("ResultNoisiestNone", "없음"));

		if (Txt_NoisiestDetail)
		{
			// 시안의 "소음 발생 14회" 는 만들 수 없다 — 발생 횟수를 세는 곳이 없고
			// 집계되는 값은 누적 경계도 기여량뿐이다 (Txt_NoisiestDetail 주석)
			Txt_NoisiestDetail->SetText(Noisiest
				? FText::Format(LOCTEXT("ResultNoisiestDetail", "경계도 {0}% 유발 — 이 작업 최다"),
								FText::AsNumber(FMath::RoundToInt(Contribution)))
				: LOCTEXT("ResultNoisiestDetailNone", "아무도 소음을 내지 않았다"));
		}
	}

	if (Txt_LootList)
	{
		Txt_LootList->SetText(BuildLootList());
	}

	if (Txt_PlayerList)
	{
		Txt_PlayerList->SetText(BuildPlayerList());
	}

	if (Txt_TargetRatio)
	{
		const int32 Target = GS->GetTargetValue();

		// 목표가 0 인 맵(테스트 레벨)에서는 비율이 의미가 없다.
		// 0 으로 나누는 것보다 문구를 비우는 편이 정직하다
		Txt_TargetRatio->SetVisibility(Target > 0 ? ESlateVisibility::SelfHitTestInvisible
												  : ESlateVisibility::Collapsed);

		if (Target > 0)
		{
			const int32 Percent = FMath::RoundToInt(
				static_cast<float>(GS->GetLoadedValue()) / static_cast<float>(Target) * 100.f);

			Txt_TargetRatio->SetText(FText::Format(
				LOCTEXT("ResultTargetRatio", "목표 {0} 대비 {1}%"),
				HeavyUIText::Money(Target), FText::AsNumber(Percent)));
		}
	}

	// 컨테이너가 있으면 행 위젯으로도 그린다. 없으면 아무것도 하지 않는다
	PopulateLootRows();
	PopulatePlayerRows();

	UE_LOG(LogHeavyUI, Log, TEXT("%s: 결과 표시 — 등급 %s, 적재 %d건 $%d / $%d, 소요 %.1f초"),
		   *GetName(), HeistOutcome::ToString(Outcome),
		   GS->GetLoadedEntries().Num(), GS->GetLoadedValue(), GS->GetTargetValue(),
		   GS->GetElapsedSeconds());

	OnResultShown(Outcome);
}

FText UHeistResultWidget::BuildLootList() const
{
	const AHeistGameState* GS = BoundState.Get();
	if (!GS)
	{
		return FText::GetEmpty();
	}

	TArray<FLootLine> Lines;
	CollectLootLines(*GS, Lines);

	if (Lines.IsEmpty())
	{
		return LOCTEXT("ResultNoLoot", "실어온 것이 없다");
	}

	TArray<FString> Rows;
	Rows.Reserve(Lines.Num());

	for (const FLootLine& Line : Lines)
	{
		Rows.Add(FText::Format(LOCTEXT("ResultLootRow", "{0}    {1}"),
							   LootLineLabel(Line), HeavyUIText::Money(Line.Value)).ToString());
	}

	return FText::FromString(FString::Join(Rows, TEXT("\n")));
}

FText UHeistResultWidget::BuildPlayerList() const
{
	const AHeistGameState* GS = BoundState.Get();
	if (!GS)
	{
		return FText::GetEmpty();
	}

	TArray<FPlayerLine> Lines;
	CollectPlayerLines(*GS, Lines);

	if (Lines.IsEmpty())
	{
		return FText::GetEmpty();
	}

	TArray<FString> Rows;
	Rows.Reserve(Lines.Num());

	for (const FPlayerLine& Line : Lines)
	{
		Rows.Add(FText::Format(LOCTEXT("ResultPlayerRow", "{0}    {1}    {2}"),
							   FText::FromString(Line.Name),
							   HeavyUIText::Money(Line.Value),
							   Line.Status).ToString());
	}

	return FText::FromString(FString::Join(Rows, TEXT("\n")));
}

// ──────────────────────────────────────────────────────────────
// 목록 — 행 위젯 (시안 sneakers_result_screen)
// ──────────────────────────────────────────────────────────────

void UHeistResultWidget::PopulateLootRows()
{
	// 컨테이너나 행 클래스 중 하나라도 없으면 여러 줄 텍스트 쪽이 그린다.
	// 둘 다 Optional 이라 WBP 를 한 번에 갈아엎지 않아도 된다
	if (!Box_LootList || !LootRowClass)
	{
		return;
	}

	Box_LootList->ClearChildren();

	const AHeistGameState* GS = BoundState.Get();
	if (!GS)
	{
		return;
	}

	TArray<FLootLine> Lines;
	CollectLootLines(*GS, Lines);

	for (const FLootLine& Line : Lines)
	{
		UResultRowWidget* Row = CreateWidget<UResultRowWidget>(this, LootRowClass);
		if (!Row)
		{
			continue;
		}

		// 음수 비율은 '이 목록에 바가 없다' 는 뜻이다 (UResultRowWidget::SetRow 주석).
		// 파손 · 유출로 깎인 줄은 흐리게 — 값이 낮다는 사실만으로는 사고였는지 알 수 없다
		Row->SetRow(LootLineLabel(Line),
					HeavyUIText::Money(Line.Value),
					/*Ratio=*/-1.f,
					/*bDimmed=*/Line.Value < Line.BaseValue);

		Box_LootList->AddChild(Row);
	}

	UE_LOG(LogHeavyUI, Log, TEXT("%s: 적재 목록 %d행"), *GetName(), Lines.Num());
}

void UHeistResultWidget::PopulatePlayerRows()
{
	if (!Box_PlayerList || !PlayerRowClass)
	{
		return;
	}

	Box_PlayerList->ClearChildren();

	const AHeistGameState* GS = BoundState.Get();
	if (!GS)
	{
		return;
	}

	TArray<FPlayerLine> Lines;
	CollectPlayerLines(*GS, Lines);

	// [분모를 1등이 아니라 총액으로 잡는 이유] 1등 기준으로 정규화하면 1등 바가 항상
	// 가득 차서, 혼자 다 벌어온 판과 넷이 고르게 나눈 판이 똑같아 보인다.
	// 총액 기준이면 바 길이의 합이 곧 전체가 되어 '얼마나 몰렸는가' 가 읽힌다.
	const int32 Total = GS->GetLoadedValue();

	for (const FPlayerLine& Line : Lines)
	{
		UResultRowWidget* Row = CreateWidget<UResultRowWidget>(this, PlayerRowClass);
		if (!Row)
		{
			continue;
		}

		const float Ratio = (Total > 0) ? static_cast<float>(Line.Value) / static_cast<float>(Total) : 0.f;

		// 금액이 아니라 퍼센트를 쓴다 — 시안이 그렇고, 바와 같은 값을 두 번 말해야
		// 바가 무엇을 재는 것인지 읽힌다. 절대 금액은 적재 목록에 이미 있다
		Row->SetRow(FText::FromString(Line.Name),
					FText::Format(LOCTEXT("ResultShare", "{0}%"),
								  FText::AsNumber(FMath::RoundToInt(Ratio * 100.f))),
					Ratio,
					/*bDimmed=*/false);

		Box_PlayerList->AddChild(Row);
	}

	UE_LOG(LogHeavyUI, Log, TEXT("%s: 기여도 %d행 (총액 $%d)"), *GetName(), Lines.Num(), Total);
}

// ──────────────────────────────────────────────────────────────
// 확인 버튼과 카운트다운
// ──────────────────────────────────────────────────────────────

void UHeistResultWidget::HandleConfirmClicked()
{
	if (bLocalConfirmed)
	{
		return;
	}

	AHeistPlayerController* PC = Cast<AHeistPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		// 이 화면에서 유일하게 조용히 실패할 수 있는 지점이다 —
		// 버튼은 눌리는데 아무 일도 안 일어난다
		UE_LOG(LogHeavyUI, Warning,
			   TEXT("%s: 확인 버튼이 서버로 가지 못했다. GameMode 의 PlayerControllerClass 를 "
					"AHeistPlayerController(또는 그 자식 BP)로 지정할 것"),
			   *GetName());
		return;
	}

	// 서버 응답을 기다리지 않고 바로 잠근다. 왕복하는 동안 여러 번 눌리면
	// 같은 확인이 여러 번 날아가고, 화면은 그동안 눌리지 않은 것처럼 보인다
	bLocalConfirmed = true;

	// develop 의 정식 컨트롤러가 확인을 켜고 끌 수 있게 열어 두었다.
	// 이 화면은 취소를 제공하지 않으므로 항상 true 다
	PC->Server_SetResultConfirmed(true);

	ApplyConfirmVisual();
	OnLocalConfirmed();
}

void UHeistResultWidget::HandleConfirmCountChanged(int32 InNumConfirmed, int32 InNumPlayers)
{
	ConfirmedCount = InNumConfirmed;
	PlayerCount    = InNumPlayers;

	ApplyConfirmVisual();

	OnConfirmCountChanged(ConfirmedCount, PlayerCount);
}

void UHeistResultWidget::RefreshCountdown()
{
	const AHeistGameState* GS = BoundState.Get();
	if (!GS)
	{
		return;
	}

	float Remaining = 0.f;
	if (!GS->TryGetPhaseRemainingSeconds(Remaining))
	{
		// 체류 시간이 0 이면 전원 확인만 기다린다 (UHeistSettings::ResultSeconds 주석)
		LastShownSeconds = INDEX_NONE;
		ApplyConfirmVisual();
		return;
	}

	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(Remaining));
	if (Seconds == LastShownSeconds)
	{
		return;
	}

	LastShownSeconds = Seconds;
	ApplyConfirmVisual();
}

void UHeistResultWidget::ApplyConfirmVisual()
{
	if (Btn_Confirm)
	{
		// 이미 확인한 사람이 다시 누를 이유가 없다. 비활성화가 "내가 눌렀다" 의 신호이기도 하다
		Btn_Confirm->SetIsEnabled(!bLocalConfirmed);
	}

	if (!Txt_Confirm)
	{
		return;
	}

	// 남은 초를 아는 동안에는 그것이 가장 중요한 정보다 — 언제 화면이 넘어가는지가
	// 확인 인원보다 급하다. 카운트다운이 없을 때만 인원을 보여준다
	if (LastShownSeconds != INDEX_NONE)
	{
		Txt_Confirm->SetText(bLocalConfirmed
			? FText::Format(LOCTEXT("ResultConfirmWaiting", "{0}/{1} 확인   ({2})"),
							FText::AsNumber(ConfirmedCount), FText::AsNumber(PlayerCount),
							FText::AsNumber(LastShownSeconds))
			: FText::Format(LOCTEXT("ResultConfirmCountdown", "로비로 돌아가기 ({0})"),
							FText::AsNumber(LastShownSeconds)));
		return;
	}

	Txt_Confirm->SetText(FText::Format(LOCTEXT("ResultConfirmCount", "{0}/{1} 확인"),
									   FText::AsNumber(ConfirmedCount), FText::AsNumber(PlayerCount)));
}

// ──────────────────────────────────────────────────────────────
// 조회
// ──────────────────────────────────────────────────────────────

EHeistOutcome UHeistResultWidget::GetOutcome() const
{
	const AHeistGameState* GS = BoundState.Get();
	return GS ? GS->GetOutcome() : EHeistOutcome::Failure;
}

FText UHeistResultWidget::GetOutcomeText() const
{
	return OutcomeToTitle(GetOutcome());
}

FLinearColor UHeistResultWidget::GetOutcomeColor() const
{
	switch (GetOutcome())
	{
	case EHeistOutcome::Success:
		return UUISettings::GetUIColor(EUIColorToken::GoldBright);

	case EHeistOutcome::Partial:
		return UUISettings::GetUIColor(EUIColorToken::Gold);

	default:
		// 실패는 경보 색을 그대로 쓴다. 색을 새로 만들면 팔레트만 늘어난다
		return UUISettings::Get()->AlarmColor;
	}
}

#undef LOCTEXT_NAMESPACE
