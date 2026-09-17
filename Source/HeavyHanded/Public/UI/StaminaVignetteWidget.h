#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"   // FGameplayTag — 태그 콜백 시그니처에 값으로 들어간다
#include "StaminaVignetteWidget.generated.h"

class UAbilitySystemComponent;
class UImage;
class UWidgetAnimation;

/** 맥동 상태. 애니메이션을 매 갱신마다 다시 재생하지 않으려고 현재 상태를 들고 있는다 */
enum class EStaminaPulse : uint8
{
	None,
	/** 숨이 차기 시작. WBP 가 저작한 속도 그대로 */
	Soft,
	/** 고갈. 같은 애니메이션을 ExhaustedPulseSpeed 배로 돌린다 */
	Hard
};

/**
 * 스태미나 화면 이펙트 (기획서 4장 조작 / 8장 UI).
 *
 * [바를 만들지 않는다] UDamageVignetteWidget 이 체력바 없이 핏자국만 던지는 것과
 *   같은 이유다. 스태미나 바를 화면에 상시로 띄우면 4인 협동 잠입에서 아무도
 *   보지 않는 위젯이 시야만 차지한다. 잔량은 화면이 얼마나 어두운가로 읽는다.
 *   WBP_HUD 의 Bar_Stamina 는 이 위젯이 들어오면서 빠진다.
 *
 * [달릴 때가 아니라 바닥날 때 뜬다] 스태미나는 피격과 달리 사건이 아니라 상태다.
 *   달리는 내내 화면을 덮으면 잠입이 불가능해지므로, LowStaminaThreshold01
 *   아래로 떨어졌을 때만 뜨고 그 위 구간에서는 아예 그리지 않는다(Collapsed).
 *
 * [고갈 중에는 잔량을 보지 않는다] State.Exhausted 가 붙어 있는 동안은 강도를 1 로
 *   고정한다. 회복은 태그와 무관하게 곧바로 시작되므로(ABaseCharacter::TickStaminaRegen)
 *   잔량에 비례시키면 아직 Shift 가 안 먹는데 화면이 먼저 걷힌다.
 *   바가 없는 설계에서는 화면이 걷히는 순간이 "다시 뛸 수 있다" 는 유일한 신호다.
 *
 * [읽기만 한다] ASC 의 어트리뷰트와 태그를 구독할 뿐 아무것도 바꾸지 않는다
 *   (규약 01 — UI 는 시스템 상태를 읽고 구독만 한다).
 *
 * [스프린트 태그를 구독하지 않는 이유] 화면에 나가는 것이 잔량과 고갈 두 가지뿐이라
 *   State.Sprinting 은 판단에 쓰이지 않는다. 달리는 동안의 숨소리처럼 그 태그가
 *   필요한 연출이 생기면 그때 구독을 하나 더 연다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UStaminaVignetteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 0~1 강도. 화면에 실제로 적용된 값이라 보간 중이면 목표와 다르다.
	 *
	 * 카메라 셰이크 Scale · 호흡 사운드 볼륨처럼 이 위젯 밖의 연출도 같은 값
	 * 하나를 받아 쓰라고 열어 둔다 (UDamageVignetteWidget::GetIntensity 와 같은 자리).
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Stamina")
	float GetIntensity() const { return DisplayedIntensity; }

	/** 0~1 스태미나 잔량. 못 붙었으면 1(= 이펙트 없음) */
	UFUNCTION(BlueprintPure, Category = "UI|Stamina")
	float GetStamina01() const;

	/** State.Exhausted 가 붙어 있는가. 이 동안은 스프린트가 막혀 있다 */
	UFUNCTION(BlueprintPure, Category = "UI|Stamina")
	bool IsExhausted() const;

	/**
	 * 실제 스태미나를 무시하고 화면을 강제한다. 치트(hh.UI.Stamina)가 쓴다.
	 *
	 * 40% 아래를 눈으로 보려면 매번 숨이 찰 때까지 달려야 해서 확인 비용이 크다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Stamina")
	void SetDebugState(float NewStamina01, bool bNewExhausted);

	/** 강제 상태를 풀고 실제 값으로 돌아간다 */
	UFUNCTION(BlueprintCallable, Category = "UI|Stamina")
	void ClearDebugState();

protected:
	//~ UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──

	/**
	 * 피로 이펙트 이미지. 앵커는 풀스크린.
	 *
	 * [반드시 Hit Test Invisible 로 둘 것] 화면을 덮는 이미지가 클릭을 먹으면
	 *   결과 화면 버튼이 눌리지 않는다 (UDamageVignetteWidget 과 같은 함정이다).
	 *
	 * 가운데는 비우고 아래 가장자리에 몰린 그라디언트로 만든다. 색은 붉은 계열을
	 * 피할 것 — 피격 핏자국과 겹치면 "맞았나?" 로 읽힌다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Stamina", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Fatigue;

	/**
	 * 숨이 차는 맥동. 루프로 저작하고, 고갈에서는 같은 것을 배속으로 돌린다.
	 *
	 * [Img_Fatigue 의 RenderOpacity 를 건드리지 말 것] 그 값은 C++ 이 강도에 맞춰
	 *   덮어쓴다. 맥동은 색 틴트(ColorAndOpacity)나 스케일처럼 다른 채널을 흔들어야
	 *   서로 싸우지 않는다.
	 *
	 * 없어도 알파는 그대로 동작한다 — 맥동만 빠진다.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Stamina", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Breathe;

	// ── 튜닝 ──
	//
	// 기획서에 스태미나 수치가 없다. 전부 플레이테스트로 정할 값이라 열어 둔다.

	/** 이 잔량 아래부터 화면이 어두워지기 시작한다. 위 구간에서는 아무것도 그리지 않는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina Vignette", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float LowStaminaThreshold01 = 0.4f;

	/**
	 * 가장 어두울 때의 알파.
	 *
	 * [0.35 에서 올린 값이다] 인게임에서 실제로 달려 보니 희미해서 안 읽혔다.
	 *   플레이어가 오래 머무는 구간은 40~15% 인데 거기서는 강도가 0~0.6 이라,
	 *   최대치가 0.35 면 실제 알파가 0.2 를 넘지 못한다. 최대치만 보고 정하면
	 *   정작 대부분의 시간 동안 안 보이는 값이 나온다.
	 *
	 * 더 올릴 때는 잠입 중 바닥과 벽 경계가 보이는지를 기준으로 본다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacity = 0.6f;

	/** 이 잔량 아래부터 맥동을 켠다. 숫자가 없으니 "얼마 안 남았다" 를 리듬이 대신 말한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PulseStaminaThreshold01 = 0.15f;

	/** 고갈 구간의 맥동 배속. Breathe 를 이 배로 돌린다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina Vignette", meta = (ClampMin = "0.1", ClampMax = "8.0"))
	float ExhaustedPulseSpeed = 2.5f;

	/**
	 * 화면이 실제 값을 따라가는 속도(1/초). 0 이면 보간 없이 즉시 반영한다.
	 *
	 * [보간이 필요한 이유] 회복이 StaminaRegenInterval(0.5초)마다 StaminaRegenPerTick(2.5)씩
	 *   붙는 계단식이다. 값을 그대로 알파에 꽂으면 화면이 0.5초마다 툭툭 튄다.
	 *   고갈이 풀리는 순간의 강도 1 → 잔량 비례로 떨어지는 낙차도 이것이 받아 준다.
	 *
	 * 잔량이 아니라 강도를 보간한다 — 두 입력(잔량 · 고갈 태그)이 하나의 값으로
	 * 합쳐진 뒤라서 어느 쪽이 바뀌어도 화면은 한 가지 방식으로만 움직인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina Vignette", meta = (ClampMin = "0.0"))
	float IntensityInterpSpeed = 6.f;

	// ── BP 연출 훅 ──
	//
	// 표시는 C++ 이 이미 끝냈다. 여기는 사운드 · 카메라 셰이크 · 포스트프로세스 자리다.
	// 훅은 클라에서도 돌기 때문에 게임 상태를 바꾸지 말 것.

	/** 화면 강도가 바뀌었다. 0 이면 이펙트가 완전히 걷힌 것이다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Stamina")
	void OnIntensityUpdated(float Intensity);

	/**
	 * 고갈 상태가 켜지거나 꺼졌다.
	 *
	 * 꺼지는 순간이 "다시 뛸 수 있다" 는 신호다 — 숨을 고르는 소리를 붙인다면 여기다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Stamina")
	void OnExhaustedChanged(bool bIsExhausted);

private:
	/** 붙을 때까지 재시도한다. PlayerState 는 클라에 늦게 온다 */
	void TryBind();

	/** 구독을 전부 뗀다. 위젯이 사라진 뒤에 콜백이 오면 파괴된 이미지를 건드린다 */
	void Unbind();

	void HandleStaminaChanged(const struct FOnAttributeChangeData& Data);
	void HandleMaxStaminaChanged(const struct FOnAttributeChangeData& Data);
	void HandleExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 잔량 · 고갈 → 목표 강도. 입력이 바뀔 때마다 여기 한 곳에서만 계산한다 */
	void RefreshTarget();

	/** 보간 1스텝. 목표에 닿으면 스스로 타이머를 끈다 */
	void StepInterp();
	void StartInterp();
	void StopInterp();

	/** 지금 DisplayedIntensity 로 이미지를 그린다 */
	void ApplyVisual();

	/** 맥동 상태가 바뀌었을 때만 애니메이션을 다시 건다 */
	void UpdatePulse();

	/** 화면에 쓸 잔량. 치트가 켜져 있으면 그 값이다 */
	float GetEffectiveStamina01() const;

	/** 화면에 쓸 고갈 여부. 치트가 켜져 있으면 그 값이다 */
	bool GetEffectiveExhausted() const;

	/** 구독 중인 ASC. APlayerSessionState 가 갖고 있다(폰이 아니다) */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;
	FDelegateHandle ExhaustedTagHandle;

	FTimerHandle BindRetryHandle;
	FTimerHandle InterpHandle;

	/** 마지막으로 받은 실제 값. 비율은 그릴 때 계산한다 */
	float StaminaValue = 0.f;
	float MaxStaminaValue = 0.f;

	/** State.Exhausted 보유 여부 */
	bool bExhausted = false;

	float TargetIntensity = 0.f;
	float DisplayedIntensity = 0.f;

	EStaminaPulse ActivePulse = EStaminaPulse::None;

	/** 치트로 강제된 상태인가 */
	bool bDebugOverride = false;
	float DebugStamina01 = 1.f;
	bool bDebugExhausted = false;

	/** 바인딩 재시도에 쓴 누적 시간 */
	float BindElapsed = 0.f;

	/** 포기 경고를 한 번만 남기기 위한 플래그 */
	bool bWarnedNoASC = false;

	/** 보간 갱신 주기(초). NativeTick 이 아닌 이유는 UAlertGaugeWidget::InterpInterval 주석에 있다 */
	static constexpr float InterpInterval = 1.f / 60.f;

	/** 이 차이 아래로 좁혀지면 목표에 스냅한다. 없으면 영원히 미세하게 수렴만 한다 */
	static constexpr float InterpSnapTolerance = 0.002f;

	/** 이 강도 아래면 이미지를 통째로 숨긴다. 안 보이는 것을 그리지 않는다 */
	static constexpr float VisibleThreshold = 0.001f;

	/** 재바인딩 시도 간격 */
	static constexpr float BindRetryInterval = 0.25f;

	/**
	 * 이만큼 지나도 못 붙으면 포기한다.
	 * GAS 가 없는 테스트 맵에서 영원히 도는 타이머를 남기지 않기 위해서다.
	 */
	static constexpr float BindGiveUpSeconds = 10.f;
};
