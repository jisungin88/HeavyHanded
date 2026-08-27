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

	/** 지금까지 밴에 실은 금액($) */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	int32 GetLoadedValue() const;

	/** 이 장소의 목표 금액($) */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	int32 GetTargetValue() const;

	/** 지금 경고 상태인가. 색 전환 · 펄스 조건. 도주 페이즈이거나 남은 시간이 얼마 없을 때다 */
	UFUNCTION(BlueprintPure, Category = "UI|Heist")
	bool IsUrgent() const { return bUrgent; }

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

	// ── BP 연출 훅 ──
	//
	// 필수 표시는 C++ 이 이미 끝냈다. 여기는 화면 흔들림 · 사운드처럼
	// C++ 이 다룰 수 없는 연출 자리다. 게임 상태를 바꾸지 말 것.

	/** 페이즈가 바뀌었다. 최초 바인딩 시에도 한 번 불리며 이때 OldPhase 는 비어 있다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Heist")
	void OnPhaseUpdated(FGameplayTag NewPhase, FGameplayTag OldPhase);

	/** 적재 금액이 바뀌었다 */
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

	void ApplyObjective(int32 LoadedValue, int32 TargetValue);
	/**
	 * 경고 상태를 켜고 끈다.
	 *
	 * @param Cause  왜 바뀌었는가. 로그 전용이라 로컬라이즈하지 않는다.
	 *               빨간 타이머만 보고는 "도주라서" 와 "시간이 없어서" 를 구분할 수 없다
	 */
	void SetUrgent(bool bNewUrgent, const TCHAR* Cause);

	/** 타이머 · 목표 금액을 통째로 보이거나 숨긴다 */
	void SetHeistWidgetsVisible(bool bVisible);

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

	/** 바인딩 재시도에 쓴 누적 시간 */
	float BindElapsed = 0.f;

	bool bUrgent = false;

	/** 포기 경고를 한 번만 남기기 위한 플래그 */
	bool bWarnedNoHeistState = false;
};
