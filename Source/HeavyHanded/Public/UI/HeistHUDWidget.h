#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"      // FGameplayTag — UFUNCTION 파라미터라 전방 선언 불가
#include "Core/HeistPhase.h"           // EHeistPhaseReason — 델리게이트 콜백 시그니처에 들어간다
#include "HeistHUDWidget.generated.h"

class AHeistGameState;
class UTextBlock;
class UWidgetAnimation;
class UImage;
class UProgressBar;
class UTexture2D;

/**
 * 인게임 HUD 본체 (기획서 8장). WBP_HUD 의 C++ 베이스다.
 *
 * [무엇을 맡나] 미션 타이머와 목표 금액. 둘 다 AHeistGameState 하나에서 나오고
 *   화면에서도 붙어 있어서 한 클래스가 갖는다. 경계도 게이지는 자기 위젯
 *   (UAlertGaugeWidget)이 따로 구독하므로 여기서 건드리지 않는다.
 *
 * [남은 시간을 구독하지 않는 이유] 남은 초는 복제되지 않는다 — 복제되는 것은
 *   '끝나는 시각' 하나뿐이고 남은 시간은 각자 계산한다. 그래서 페이즈 전환만 구독하고
 *   숫자는 타이머로 주기적으로 다시 물어본다.
 *
 * [작업 레벨이 아닐 때] AHeistGameState 는 저택 · 박물관 · 은행에만 있다.
 *   GuardTest 같은 맵에서는 붙을 대상이 없으므로, 잠시 기다린 뒤 포기하고
 *   타이머 · 목표 금액을 숨긴다. 빈 값이 화면에 남아 있는 것보다 낫다.
 *
 * [C++ 과 WBP 의 경계] 문자열 · 색 · 가시성은 C++ 이 정한다.
 *   WBP 는 배치와 애니메이션 저작만 한다.
 *
 * [진행도 바가 없는 이유] 기획서 8장은 "현재/목표 금액" 수치만 요구한다.
 *   바는 UStatBarWidget(체력 · 스태미나 · 무게 공용 베이스)을 만들 때 같이 붙인다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UHeistHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 현재 페이즈가 끝나기까지 남은 초.
	 *
	 * @return 카운트다운이 있으면 true. 접속 대기는 false
	 *
	 * [Result 도 true 다] 결과 페이즈에는 체류 시간(UHeistSettings::ResultSeconds)이
	 *   걸려 있어서 여기서도 값이 나온다. 그건 결과 화면이 그릴 값이지 미션 타이머가
	 *   아니므로, 화면에 쓰는 쪽(RefreshTimer)이 페이즈를 보고 따로 걸러낸다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	bool TryGetRemainingSeconds(float& OutSeconds) const;

	/** 현재 페이즈 태그. 붙지 않았으면 빈 태그 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	FGameplayTag GetCurrentPhase() const;

	/**
	 * 현재 페이즈의 화면 문구 ("준비" · "본 작업" · "경찰 도착까지" · "결과").
	 *
	 * 도주는 들어온 사유에 따라 문구가 갈린다 — 경보면 "경찰 도착까지",
	 * 제한 시간 만료면 "탈출까지". 페이즈 이름을 그대로 쓰지 않는 이유다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	FText GetPhaseLabel() const;

	/** 지금까지 밴에 실은 금액($). 화면에서 굴러가는 중간값이 아니라 서버가 알려준 실제 값이다 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	int32 GetLoadedValue() const;

	/**
	 * 지금 화면에 찍혀 있는 금액($). 롤업 중이면 GetLoadedValue() 보다 작다.
	 *
	 * 연출 전용이다 — 목표 달성 여부 같은 판단에 쓰지 말 것.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	int32 GetDisplayedLoadedValue() const { return FMath::RoundToInt(DisplayedLoaded); }

	/** 지금 금액이 굴러가는 중인가. WBP 가 틱 사운드를 깔 때 쓴다 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	bool IsMoneyRolling() const { return MoneyInterpHandle.IsValid(); }

	/** 이 장소의 목표 금액($) */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	int32 GetTargetValue() const;

	/** 지금 경고 상태인가. 색 전환 · 펄스 조건. 도주 페이즈이거나 남은 시간이 얼마 없을 때다 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	bool IsUrgent() const { return bUrgent; }

	/**
	 * 준비 카운트다운을 화면에서만 흉내 낸다. 치트(hh.UI.PrepStart · hh.UI.PrepFinal)가 쓴다.
	 *
	 * 게임 상태는 건드리지 않는다 — 페이즈도 서버 시각도 그대로다. 끝나면 "시작!" 까지 보여 주고
	 * 스스로 풀린다. 알림은 준비 시작 2초에만 나와서, 확인하려면 매번 판을 새로 열어야 했다.
	 *
	 * @param bFromStart  true 면 준비 처음(PrepSeconds)부터, false 면 마지막 PrepFinalSeconds 초부터
	 */
	void StartDebugPrep(bool bFromStart);

protected:
	//~ UUserWidget
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──
	//
	// 이름은 WBP_HUD 에 이미 있는 것을 그대로 쓴다.

	/** 남은 시간 ("6:52") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Heist", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Timer;

	/**
	 * Txt_Timer 를 감싼 판(배경 · 그림자 · 광택). 있으면 타이머를 숨길 때 이것째로 숨긴다 —
	 * 글자만 숨기면 빈 판이 화면에 남는다. 판 없이 글자만 쓰는 WBP 도 있어서 Optional
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Heist", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Timer;

	/**
	 * 타이머 판과 그 아래 받침. 위급할 때 이 둘을 경보 색으로 바꾼다.
	 *
	 * 글자를 빨갛게 하는 대신 판을 바꾸는 이유 — 어두운 판 위의 빨간 글자는 대비가 낮아
	 * 정작 위급할 때 숫자가 흐려진다. 판이 있으면 글자는 흰색 그대로 둔다.
	 * 둘 다 없으면 예전처럼 글자 색만 바꾼다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Heist", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Plate;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Heist", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_PlateShadow;

	// ── 준비 카운트다운 ──
	//
	// 준비 45초는 미션 타이머 판에 띄우지 않는다. 대신 네 단계로 보여 준다.
	//   ① 준비 시작 PrepAnnounceSeconds 동안 — 가운데 크게 "작전 준비 / 45초"
	//   ② 그다음 — 위쪽에 작게 "작업 시작까지 0:43"
	//   ③ 남은 PrepFinalSeconds 초 — 다시 가운데 크게 "작업 시작까지 / 5"
	//   ④ 본 작업 진입 StartBannerSeconds 동안 — 가운데 "시작!"
	//
	// 판에 그대로 띄우지 않는 이유 — 0:00 에서 7:00 으로 튀어 올라, "준비" 글자를 못 본
	// 사람은 본 작업 타이머로 착각한다. 전부 Optional 이라 안 만들어 둔 WBP 도 그대로 돈다.

	/** 가운데 큰 글자 묶음 (①③④). 이것째로 숨기고 보인다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Prep", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_PrepCenter;

	/** 가운데 위 작은 글자 ("작전 준비" · "작업 시작까지"). ④ 에서는 숨는다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Prep", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_PrepCenterLabel;

	/** 가운데 큰 글자 ("45초" · "5" · "시작!") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Prep", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_PrepCenterValue;

	/** 위쪽 작은 카운트다운 묶음 (②). "작업 시작까지" 글자는 WBP 에 고정으로 둔다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Prep", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_PrepMini;

	/** ② 의 남은 시간 ("0:43") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Prep", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_PrepMini;

	/** 가운데 글자가 바뀔 때마다 재생한다 (숫자가 톡 커지는 연출). 없으면 글자만 바뀐다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Prep", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> PrepPop;

	/**
	 * 페이즈 이름 ("본 작업"). 아직 WBP 에 없어서 Optional 이다 —
	 * 추가하면 이름만 맞춰 두면 자동으로 채워진다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Heist", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Phase;

	/** 금액 수치 ("$18,400 / $50,000"). 위와 같은 이유로 Optional */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Heist", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Objective;

	/** 남은 시간이 얼마 없을 때 재생한다. 없으면 색만 바뀐다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Heist", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> UrgentPulse;

	// ── 소지 슬롯 (기획서 8장 "소지 노획물") ──
	//
	// [이름을 전부 새로 잡은 이유] WBP_HUD 에 있던 Panel_HeldLoot · Bar_Weight ·
	//   Txt_HeldLoot 은 쓰지 않는다. 같은 이름의 위젯에 '변수 여부' 가 켜져 있으면
	//   BP 컴파일이 "another object already exists there" 로 실패하고, 그 상태로
	//   플레이하면 프로퍼티 레이아웃이 꼬여 포인터가 쓰레기값이 된다
	//   (2026-08-25 Bar_Weight 로 두 번 크래시). 새 이름은 BP 변수가 만들어진 적이
	//   없어서 그 상태에 빠지지 않는다.
	//
	// 전부 Optional 이다 — WBP 에 아직 없어도 나머지 HUD 는 그대로 떠야 한다.

	/** 슬롯 전체. 아무것도 안 들고 있으면 이것만 숨기면 된다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Held", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Held;

	/** 특성 아이콘. UUISettings::GetHeldSlotIcon() 이 고른다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Held", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Held;

	/** 표시 이름 ("도자기 세트"). DT_LootCatalog 의 DisplayName */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Held", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_HeldName;

	/** 무게 바. 분모는 UUISettings::HeldWeightBarMaxKg — 표시 전용 값이다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Held", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_HeldWeight;

	/** 무게와 가치 ("24kg · $8,000") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Held", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_HeldInfo;

	/**
	 * 이 시간 이하로 남으면 경고 상태가 된다.
	 *
	 * 기획서에 수치가 없어서 잡은 값이다.
	 *
	 * [도주에는 적용되지 않는다] 도주 90초는 진입 순간부터 끝까지 경고 상태다.
	 *   그래서 이 값은 준비 · 본 작업에서 "시간이 얼마 안 남았다" 를 알리는 용도로만 쓰인다.
	 *   본 작업 제한 시간(7~9분)보다 충분히 작게 둘 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist HUD", meta = (ClampMin = "0.0", Units = "s"))
	float UrgentSeconds = 30.f;

	/**
	 * 표시 금액이 실제 금액을 따라가는 속도(1/초). 0 이면 롤업 없이 즉시 반영한다.
	 *
	 * 남은 차액에 비례해 좁히는 방식이라(FInterpTo) 처음이 빠르고 끝이 느리다.
	 * 6 이면 큰 금액이든 작은 금액이든 0.5~1초 안에 도착한다 — 액수에 따라 시간이
	 * 크게 달라지지 않는 것이 이 방식을 고른 이유다. 밴 적재는 연달아 일어난다.
	 *
	 * UAlertGaugeWidget::BarInterpSpeed 와 같은 뜻이고 기본값도 같게 맞췄다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist HUD", meta = (ClampMin = "0.0"))
	float MoneyInterpSpeed = 6.f;

	/** 준비 시작 후 "작전 준비 45초" 를 가운데 띄워 두는 시간. 0 이면 알림 없이 바로 작은 카운트다운 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist HUD|Prep", meta = (ClampMin = "0.0", Units = "s"))
	float PrepAnnounceSeconds = 2.f;

	/** 준비가 이만큼 남으면 다시 가운데 크게 센다 (5 · 4 · 3 · 2 · 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist HUD|Prep", meta = (ClampMin = "0"))
	int32 PrepFinalSeconds = 5;

	/** 본 작업 진입 때 "시작!" 을 띄워 두는 시간. 0 이면 안 띄운다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist HUD|Prep", meta = (ClampMin = "0.0", Units = "s"))
	float StartBannerSeconds = 1.f;

	/**
	 * 가운데 큰 글자 크기 — 타이머 폰트(TimerFont)에 곱한다.
	 * 전용 폰트 토큰을 새로 만들지 않는다. 폰트 3단계를 유지하려는 것이다 (HeldSlotFontScale 과 같은 방식)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heist HUD|Prep", meta = (ClampMin = "0.1"))
	float PrepCenterFontScale = 2.2f;

	// ── BP 연출 훅 ──
	//
	// 필수 표시는 C++ 이 이미 끝냈다. 여기는 화면 흔들림 · 사운드처럼
	// C++ 이 다룰 수 없는 연출 자리다. 게임 상태를 바꾸지 말 것.

	/** 페이즈가 바뀌었다. 최초 바인딩 시에도 한 번 불리며 이때 OldPhase 는 비어 있다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Heist")
	void OnPhaseUpdated(FGameplayTag NewPhase, FGameplayTag OldPhase);

	/**
	 * 적재 금액이 바뀌었다. 인자는 롤업 전 실제 값이다.
	 *
	 * [롤업 프레임마다 불리지 않는다] 숫자가 굴러가는 동안에는 C++ 이 글자만 갈아끼우고
	 *   이 훅은 부르지 않는다. 실제로 뭔가 실렸을 때 한 번만 오므로 여기에
	 *   펀치 애니메이션 · 코인 사운드를 걸면 된다
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Heist")
	void OnObjectiveUpdated(int32 LoadedValue, int32 TargetValue);

	/** 경고 상태가 켜지거나 꺼졌다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Heist")
	void OnUrgentChanged(bool bIsUrgent);

	/**
	 * 손에 든 것이 바뀌었다. 집으면 NewHeld 가 유효하고 놓으면 null 이다.
	 *
	 * 표시는 C++ 이 이미 끝냈다. 여기는 슬라이드 인 · 사운드 자리다
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Held")
	void OnHeldChanged(AActor* NewHeld);

private:
	UFUNCTION()
	void HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason);

	UFUNCTION()
	void HandleLoadedValueChanged(int32 LoadedValue, int32 TargetValue);

	/** 붙을 때까지 재시도한다. 작업 레벨이 아니면 결국 포기하고 숨긴다 */
	void TryBind();

	/** 주기 콜백. 남은 시간을 다시 계산해 글자에 반영한다 */
	void RefreshTimer();

	/**
	 * 준비 카운트다운 네 단계 중 지금 것을 그린다. RefreshTimer 가 매 주기 부른다.
	 *
	 * @param bPrep      지금 준비 페이즈이고 남은 시간을 알 수 있는가
	 * @param Remaining  준비 남은 초. bPrep 이 false 면 무시한다
	 */
	void RefreshPrepCountdown(bool bPrep, float Remaining);

	/** 가운데 큰 글자를 띄운다. Key 가 직전과 같으면 아무것도 안 한다 (PrepPop 이 매 주기 리셋되지 않게). 숨기는 쪽은 항상 적용한다 */
	void ShowPrepCenter(const FText& Label, const FText& Value, int32 Key);
	void HidePrepCenter();

	/** 위쪽 작은 카운트다운. INDEX_NONE 이면 숨긴다 */
	void SetPrepMini(int32 Seconds);

	/**
	 * 목표 금액 표시를 갱신한다.
	 *
	 * @param bImmediate  true 면 롤업 없이 바로 그 숫자를 찍는다.
	 *                    최초 바인딩에 쓴다 — 중간 참가 · 리스폰인데 0 부터 굴러가면
	 *                    방금 자기가 실은 것처럼 보인다
	 */
	void ApplyObjective(int32 LoadedValue, int32 TargetValue, bool bImmediate);

	/** 지금 DisplayedLoaded 값으로 글자와 색을 그린다. 숫자가 그대로면 SetText 를 건너뛴다 */
	void DrawObjective();

	/** 롤업 1스텝. 목표에 닿으면 스스로 타이머를 끈다 */
	void StepMoneyInterp();

	void StartMoneyInterp();
	void StopMoneyInterp();
	/**
	 * 경고 상태를 켜고 끈다.
	 *
	 * @param Cause  왜 바뀌었는가. 로그 전용이라 로컬라이즈하지 않는다.
	 *               빨간 타이머만 보고는 "도주라서" 와 "시간이 없어서" 를 구분할 수 없다
	 */
	void SetUrgent(bool bNewUrgent, const TCHAR* Cause);

	/** 타이머 · 목표 금액을 통째로 보이거나 숨긴다 */
	void SetHeistWidgetsVisible(bool bVisible);

	/** 타이머 글자와 판을 같이 보이거나 숨긴다 */
	void SetTimerVisible(bool bVisible);

	/**
	 * 주기 콜백. 손에 든 것이 바뀌었는지 확인한다.
	 *
	 * [임시 — 왜 구독이 아니라 폴링인가] ABaseCharacter::HeldActor 는 복제되지만
	 *   OnRep_HeldActor 가 델리게이트를 쏘지 않아 구독할 곳이 없다.
	 *   전영배 님께 요청한 FOnHeldActorChanged 가 열리면 이 타이머를 지우고
	 *   ApplyHeldSlot 을 그 델리게이트에 직접 붙인다.
	 *   (규약 08 — UI 는 읽고 구독만 한다. 폴링은 그 예외라 임시로만 둔다)
	 */
	void RefreshHeldSlot();

	/** 슬롯 내용을 그린다. Held 가 null 이면 슬롯을 통째로 숨긴다 */
	void ApplyHeldSlot(AActor* Held);

	/**
	 * 특성 태그에 맞는 아이콘을 로드해 돌려준다.
	 *
	 * 로드 실패도 null 로 캐시에 넣는다 — 안 그러면 못 찾는 그림을 0.1초마다 다시 찾는다
	 */
	UTexture2D* ResolveHeldIcon(const FGameplayTagContainer& TypeTags);

	UPROPERTY()
	TObjectPtr<AHeistGameState> BoundState;

	FTimerHandle BindRetryHandle;
	FTimerHandle TimerTickHandle;

	/** 금액 롤업 타이머. 목표에 닿으면 꺼진다 — 평소 비용이 0 인 것이 이 방식의 요점이다 */
	FTimerHandle MoneyInterpHandle;

	FTimerHandle HeldTickHandle;

	/** 에셋 경로 → 로드된 아이콘. 로드에 실패한 경로는 null 로 들어가 있다 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> HeldIconCache;

	/** 마지막으로 그린 대상. 같으면 아무것도 하지 않는다 */
	TWeakObjectPtr<AActor> LastHeldActor;

	/** 첫 틱인가. 아무것도 안 들고 시작해도 슬롯을 한 번은 숨겨야 한다 */
	bool bHeldSlotDrawn = false;

	/** 마지막에 슬롯에 내용이 있었는가. 들고 있던 것이 파괴되면 포인터만으로는 구분되지 않는다 */
	bool bHeldSlotFilled = false;

	/**
	 * 소지 슬롯 확인 주기(초).
	 *
	 * 집고 놓는 것은 사람 손이라 0.1초면 즉각으로 느껴진다.
	 * 자기 폰 하나만 보므로 4인이어도 각자 한 번씩이다
	 */
	static constexpr float HeldTickInterval = 0.1f;

	/** 재바인딩 시도 간격 */
	static constexpr float BindRetryInterval = 0.25f;

	/**
	 * 이만큼 지나도 못 붙으면 작업 레벨이 아니라고 보고 포기한다.
	 * 무한 재시도로 두면 GuardTest 같은 맵에서 영원히 도는 타이머가 남는다.
	 */
	static constexpr float BindGiveUpSeconds = 10.f;

	/**
	 * 남은 시간 갱신 주기(초).
	 *
	 * 화면에는 초 단위로만 나오지만 0.1초로 도는 이유는 초가 넘어가는 순간을
	 * 최대 0.1초 안에 잡기 위해서다. 1초 주기로 돌면 표시가 실제보다 최대 1초 늦는다.
	 * 문자열은 정수 초가 바뀔 때만 새로 만든다.
	 */
	static constexpr float TimerTickInterval = 0.1f;

	/** 마지막으로 Txt_Timer 에 쓴 정수 초. 같은 값이면 SetText 를 건너뛴다 */
	int32 LastShownSeconds = INDEX_NONE;

	/**
	 * WBP 에 찍어 둔 판 · 받침의 평소 색. 위급이 풀리면 이 색으로 되돌린다.
	 *
	 * 평소 색을 C++ 에 박지 않는 이유 — 판 색은 아직 시안 단계라 WBP 에서 바꿔 가며 본다.
	 * 여기에 숫자를 두면 WBP 를 고쳐도 위급 한 번 뒤에 C++ 색으로 덮인다.
	 */
	FLinearColor PlateNormalColor = FLinearColor::White;
	FLinearColor PlateShadowNormalColor = FLinearColor::White;

	/** 받침은 경보 색을 이 비율로 어둡게 쓴다. 판과 받침의 명도 차가 입체감이다 */
	static constexpr float UrgentShadowScale = 0.3f;

	/**
	 * 지금 가운데에 띄운 것. 1~PrepFinalSeconds 는 그 숫자, 아래 둘은 알림 · 시작이다.
	 * 같은 것을 다시 띄우지 않기 위한 값이다 — 0.1초마다 PrepPop 을 처음부터 틀면 멈춘 것처럼 보인다
	 */
	int32 LastPrepCenterKey = INDEX_NONE;
	static constexpr int32 PrepKeyAnnounce = -2;
	static constexpr int32 PrepKeyStart    = -3;

	/** 마지막으로 Txt_PrepMini 에 쓴 초 */
	int32 LastPrepMiniSeconds = INDEX_NONE;

	/** 이 월드 시각까지 "시작!" 을 띄운다. 음수면 안 띄운다 */
	float StartBannerUntil = -1.f;

	// ── 준비 카운트다운 흉내 (치트 전용) ──

	/** 흉내 준비가 끝나는 월드 시각. 음수면 흉내 중이 아니다 */
	float DebugPrepEndTime = -1.f;

	/** 흉내 준비가 아직 세는 중인가. 이 동안은 진짜 페이즈와 무관하게 준비 화면이다 */
	bool bDebugPrepCounting = false;

	FTimerHandle DebugPrepHandle;

	/** 흉내 1스텝. 준비 → "시작!" → 해제 순으로 스스로 넘어간다 */
	void StepDebugPrep();
	void StopDebugPrep();

	// ── 금액 롤업 ──
	//
	// UAlertGaugeWidget 의 막대와 같은 구조다 — 실제 값과 화면 값을 따로 들고,
	// 화면 값만 타이머로 좁힌다.

	/** 서버가 알려준 실제 적재 금액 */
	int32 TargetLoaded = 0;

	/** 이 장소의 목표 금액. 색 전환 기준이라 같이 들고 있는다 */
	int32 TargetGoal = 0;

	/** 화면에 굴러가는 값. 롤업 중이면 TargetLoaded 와 다르다 */
	float DisplayedLoaded = 0.f;

	/**
	 * 마지막으로 Txt_Objective 에 쓴 금액 · 목표.
	 * 롤업 중에는 초당 60번 도는데 표시는 정수라 대부분 같은 문자열이다.
	 * 둘 다 그대로면 FText 를 새로 만들지 않는다
	 */
	int32 LastShownLoaded = INDEX_NONE;
	int32 LastShownGoal   = INDEX_NONE;

	/** 롤업 갱신 주기(초). NativeTick 이 아닌 이유는 UAlertGaugeWidget::InterpInterval 주석에 있다 */
	static constexpr float MoneyInterpInterval = 1.f / 60.f;

	/**
	 * 이 차이 아래로 좁혀지면 목표값에 스냅한다.
	 *
	 * 화면에는 반올림한 정수만 나오므로 0.5 아래면 이미 같은 숫자다.
	 * 비례 방식이라 스냅하지 않으면 타이머가 영원히 돈다
	 */
	static constexpr float MoneySnapTolerance = 0.5f;

	/** 바인딩 재시도에 쓴 누적 시간 */
	float BindElapsed = 0.f;

	bool bUrgent = false;

	/** 포기 경고를 한 번만 남기기 위한 플래그 */
	bool bWarnedNoHeistState = false;
};
