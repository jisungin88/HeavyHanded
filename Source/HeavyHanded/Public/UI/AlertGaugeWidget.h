#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Noise/NoiseTypes.h"          // EAlertLevel — UFUNCTION 파라미터라 전방 선언 불가
#include "GameplayTagContainer.h"      // FGameplayTag — UFUNCTION 파라미터라 전방 선언 불가
#include "Core/HeistPhase.h"           // EHeistPhaseReason — 델리게이트 콜백 시그니처에 들어간다
#include "AlertGaugeWidget.generated.h"

class AGameStateBase;
class AHeistGameState;
class UAlertComponent;
class UProgressBar;
class UTextBlock;
class UWidgetAnimation;

/**
 * 경계도 게이지 HUD (기획서 8장).
 *
 * UAlertComponent 는 GameState 에 런타임 부착되고 클라이언트에는 복제로 도착한다.
 * 위젯이 먼저 만들어질 수 있어서 붙을 때까지 재시도한다 — 한 번만 찾고 포기하면
 * 클라이언트에서만 게이지가 영원히 0 으로 남는다.
 *
 * [본 작업과 도주에서만 보인다] 준비 시간의 경계도는 본 작업에 들어가는 순간
 *   ResetAlert() 로 지워지므로 그때까지 보여주는 값은 곧 사라질 숫자다.
 *   결과 화면의 경계도도 이미 끝난 판의 잔상이다. 그래서 페이즈를 구독해 스스로 숨는다.
 *   작업 레벨이 아닌 맵(L_NoiseTest · GuardTest)에는 페이즈가 없어서 계속 보인다.
 *
 * [C++ 과 WBP 의 경계]
 *   막대 채우기 · 색 · 단계 이름 · 퍼센트 · 점멸 재생은 전부 C++ 이 한다.
 *   아래 BindWidget 프로퍼티가 그 창구다 — WBP 에 같은 이름의 위젯이 없으면
 *   BP 컴파일이 실패한다. 이 프로젝트의 UI 버그는 "게이지가 안 움직인다" 처럼
 *   조용히 드러나는데, 이름 불일치만큼은 컴파일 시점에 잡아 둘 수 있다.
 *
 *   WBP 가 하는 것은 배치와 위젯 애니메이션 저작뿐이다. 값·색·문구를 WBP 에서
 *   따로 만지면 C++ 이 매 갱신마다 덮어써서 반영되지 않는다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UAlertGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 0~1. 아직 못 붙었으면 0. 보간 중인 표시값이 아니라 컴포넌트의 실제 값이다 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	float GetGauge01() const;

	/**
	 * 히스테리시스가 있어서 단계는 게이지만으로 결정되지 않는다.
	 * 게이지 65% 가 상승 중이면 의심, 하강 중이면 경계다. 직접 계산하지 말 것
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	EAlertLevel GetAlertLevel() const;

	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	FLinearColor GetLevelColor() const;

	/** 색만으로 단계를 구분하지 않기 위한 표시용 이름 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	FText GetLevelText() const;

	/** 경보(래치) 상태인가. 90초 카운트다운 표시 조건 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	bool IsAlarmed() const;

	/**
	 * 지금 페이즈에서 경계도가 의미 있는가. 작업 레벨이 아니면 항상 참이다.
	 *
	 * 본 작업과 도주에서만 참이다 — 준비 시간의 경계도는 본 작업에 들어가는 순간
	 * ResetAlert() 로 지워지고, 결과 화면의 경계도는 이미 끝난 판의 잔상이다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Alert")
	bool IsGaugeRelevant() const { return bGaugeRelevant; }

protected:
	//~ UUserWidget
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──
	//
	// 이름은 WBP_AlertGauge 에 이미 있는 것을 그대로 쓴다. 컨벤션과 어긋나 보이지만
	// (Bar_Alert 가 아니라 AlertBar 여야 자연스럽다) 이름을 바꾸면 기존 WBP 의
	// 위젯을 전부 개명해야 하고, 그 사이 BP 가 컴파일되지 않는다. 얻는 것보다 잃는 것이 크다.

	/** 경계도 막대. 채우기와 색을 C++ 이 지정한다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Alert", meta = (BindWidget))
	TObjectPtr<UProgressBar> Bar_Alert;

	/** 단계 이름("평온" · "의심" · "경계" · "경보") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Alert", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Level;

	/** 퍼센트 수치("65%") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Alert", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Percent;

	/**
	 * 경보 단계 점멸. 2Hz 로 저작한다 (UUISettings::AlarmBlinkHz 주석).
	 *
	 * Optional 인 이유는 점멸이 빠져도 게이지 자체는 성립하기 때문이다 —
	 * 스킨 WBP 마다 애니메이션 저작을 강제하면 만들 때마다 컴파일이 막힌다.
	 * 대신 경보인데 애니메이션이 없으면 로그를 남긴다 (조용히 실패하지 않게).
	 *
	 * [BlueprintReadOnly 가 필요한 이유] 위젯 애니메이션은 원래 WBP 가 만든 BP 변수라
	 * 그래프에서 자유롭게 참조된다. C++ 이 같은 이름을 잡는 순간 주인이 이 프로퍼티로
	 * 바뀌므로, 열어 두지 않으면 기존 그래프의 Get 노드가 접근 위반으로 BP 컴파일을 막는다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "UI|Alert", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> AlarmBlink;

	/**
	 * 막대가 실제 값을 따라가는 속도(1/초). 0 이면 보간 없이 즉시 반영한다.
	 *
	 * 경계도는 0.1초마다(UAlertComponent::TickInterval) 갱신되어 그대로 그리면
	 * 눈에 띄게 계단이 진다. 값이 아니라 속도인 이유는 남은 거리에 비례해 좁히기 때문에
	 * 큰 변화는 빠르게, 자연 감소는 느리게 따라가기 때문이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert Gauge", meta = (ClampMin = "0.0"))
	float BarInterpSpeed = 6.f;

	// ── BP 연출 훅 ──
	//
	// 필수 표시는 위에서 C++ 이 이미 끝냈다. 이 훅은 화면 흔들림 · 파티클처럼
	// C++ 이 다룰 수 없는 추가 연출 자리다. 여기서 게임 상태를 바꾸지 말 것 —
	// 훅은 클라이언트에서도 돈다.

	/** 게이지 값이 바뀌었다. 인자는 보간 전 실제 값이다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Alert")
	void OnGaugeUpdated(float NewGauge01);

	/**
	 * 단계가 바뀌었다.
	 * 최초 바인딩 시에도 한 번 호출되며 이때는 NewLevel == OldLevel 이다
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Alert")
	void OnLevelUpdated(EAlertLevel NewLevel, EAlertLevel OldLevel, FLinearColor LevelColor);

	/**
	 * 페이즈 때문에 게이지가 나타나거나 사라졌다.
	 *
	 * 가시성 자체는 C++ 이 이미 바꿨다. 여기는 페이드 · 사운드 자리다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Alert")
	void OnGaugeVisibilityChanged(bool bVisible);

private:
	UFUNCTION()
	void HandleGaugeChanged(float NewGauge01);

	UFUNCTION()
	void HandleLevelChanged(EAlertLevel NewLevel, EAlertLevel OldLevel);

	/** 성공할 때까지 재시도한다. 클라에서는 GameState 도 컴포넌트도 늦게 온다 */
	void TryBind();

	/**
	 * 페이즈 구독을 건다. GameState 가 아직 없으면 도착할 때 다시 불린다.
	 *
	 * 위의 TryBind() 와 달리 재시도 타이머를 쓰지 않는다 — 엔진이 GameState 가
	 * 세팅되는 순간을 UWorld::GameStateSetEvent 로 알려주기 때문이다.
	 * (TryBind 쪽은 GameState 가 온 뒤에도 컴포넌트가 더 늦게 붙을 수 있어 폴링이 남아 있다.)
	 */
	void BindToHeistGameState();

	/** UWorld::GameStateSetEvent 콜백. 작업 레벨의 GameState 면 구독을 건다 */
	void HandleGameStateSet(AGameStateBase* NewGameState);

	UFUNCTION()
	void HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason);

	/** 이 페이즈에서 게이지를 보여줄지 정한다 */
	void ApplyPhaseVisibility(FGameplayTag Phase);

	/** 게이지 전체를 보이거나 숨긴다. 바뀔 때만 실제로 움직인다 */
	void SetGaugeRelevant(bool bRelevant, const TCHAR* Cause);

	/** 표시값을 막대와 퍼센트 글자에 반영한다 */
	void ApplyGaugeVisual(float Gauge01);

	/** 단계에 맞는 색 · 이름 · 점멸을 반영한다 */
	void ApplyLevelVisual(EAlertLevel NewLevel);

	/** 경보 점멸을 켜고 끈다 */
	void UpdateAlarmBlink(bool bShouldBlink);

	/** 보간 1스텝. 목표에 닿으면 스스로 타이머를 끈다 */
	void StepInterp();

	void StartInterp();
	void StopInterp();

	UPROPERTY()
	TObjectPtr<UAlertComponent> BoundAlert;

	UPROPERTY()
	TObjectPtr<AHeistGameState> BoundState;

	/** GameStateSetEvent 구독 해제용. 다이나믹 델리게이트가 아니라 핸들로 뗀다 */
	FDelegateHandle GameStateSetHandle;

	/**
	 * 지금 페이즈에서 게이지를 보여줄 것인가.
	 *
	 * 기본값이 true 인 것은 의도다 — 작업 레벨이 아닌 맵(L_NoiseTest · GuardTest)에는
	 * 페이즈가 없어서 이 값을 갱신할 사람이 아무도 없다. 기본이 false 면 그 맵들에서
	 * 게이지가 영영 안 보이고, 원인은 화면만 봐서는 알 수 없다.
	 */
	bool bGaugeRelevant = true;

	FTimerHandle BindRetryHandle;

	/** 재바인딩 시도 간격 */
	static constexpr float BindRetryInterval = 0.25f;

	/**
	 * 보간 갱신 주기(초).
	 *
	 * NativeTick 대신 타이머를 쓴다. UUserWidget 의 TickFrequency 기본값(Auto)은
	 * BP 에 Tick 이벤트가 있거나 애니메이션이 재생 중일 때만 틱을 켜기 때문에,
	 * C++ 에서만 틱하는 위젯은 WBP 구성에 따라 NativeTick 이 아예 불리지 않을 수 있다.
	 * 타이머는 그 조건과 무관하고, 목표에 닿으면 꺼져서 평소 비용이 0 이다.
	 */
	static constexpr float InterpInterval = 1.f / 60.f;

	/** 이 차이 아래로 좁혀지면 목표값에 스냅한다. 없으면 영원히 미세하게 수렴만 한다 */
	static constexpr float InterpSnapTolerance = 0.001f;

	/** WBP 가 저작해야 하는 점멸 주기(Hz). 재생 속도 환산의 기준값이다 */
	static constexpr float AuthoredBlinkHz = 2.f;

	FTimerHandle InterpHandle;

	/** 컴포넌트가 알려준 실제 값 */
	float TargetGauge = 0.f;

	/** 화면에 그려지는 값. 보간 중이면 TargetGauge 와 다르다 */
	float DisplayedGauge = 0.f;

	/**
	 * 마지막으로 Txt_Percent 에 쓴 정수 퍼센트.
	 * 보간 중에는 초당 60번 갱신되는데 표시값은 정수라 대부분 같은 문자열이다.
	 * 바뀔 때만 SetText 를 불러 매 스텝 FText 를 새로 만들지 않는다.
	 */
	int32 LastShownPercent = INDEX_NONE;

	/** 점멸 애니메이션 부재 경고를 한 번만 남기기 위한 플래그 */
	bool bWarnedMissingBlink = false;
};
