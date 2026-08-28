#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"      // FGameplayTag — UFUNCTION 파라미터라 전방 선언 불가
#include "Core/HeistPhase.h"           // EHeistPhaseReason — 델리게이트 콜백 시그니처에 들어간다
#include "InteractPromptWidget.generated.h"

class AActor;
class AGameStateBase;
class AHeistGameState;
class APawn;
class UTextBlock;

/**
 * 상호작용 프롬프트 (기획서 8장 "조준 대상 프롬프트").
 *
 * [무엇을 맡나] 지금 무엇을 조준하고 있는지 화면 중앙 아래에 알려준다.
 *   대상이 없으면 통째로 사라진다 — 조준점(크로스헤어)은 만들지 않는다.
 *
 * [왜 위젯이 직접 트레이스하나] "지금 무엇을 보고 있는가" 를 상시로 들고 있는 곳이
 *   아직 없다. UGAB_Interact 는 E 를 누르는 순간에만 스윕한다. 그래서 표시용으로
 *   위젯이 로컬에서 따로 쏜다. 판정이 아니라 표시라서 서버 권위가 필요 없다.
 *   나중에 포커스 컴포넌트가 생기면 RefreshFocus() 안쪽만 갈아끼우면 된다.
 *
 * [본 작업과 도주에서만 뜬다] 준비 시간과 결과 화면에서는 페이즈를 보고 스스로 꺼진다.
 *   결과 화면이 떠 있는 동안에도 0.1초마다 스윕을 쏘고 "[E] 집기" 를 띄우던 것을 막는다.
 *   작업 레벨이 아닌 맵에는 페이즈가 없으므로 예전처럼 계속 동작한다.
 *
 * [1인칭이라 보정이 없다] 카메라가 스프링 암 없이 눈높이에 직결돼 있어
 *   (ABaseCharacter 생성자) 카메라 위치가 곧 눈 위치다. 화면 중앙에 보이는 것이
 *   그대로 스윕에 맞는다.
 *
 * [C++ 과 WBP 의 경계] 문구 · 색 · 가시성은 C++ 이 정한다. WBP 는 배치와 연출만.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 지금 프롬프트가 떠 있는가 */
	UFUNCTION(BlueprintPure, Category = "UI|Interact")
	bool IsPromptVisible() const { return bPromptVisible; }

	/**
	 * 지금 페이즈에서 프롬프트를 띄울 것인가. 작업 레벨이 아니면 항상 참이다.
	 *
	 * 본 작업과 도주에서만 참이다. 거짓이면 시선 스윕 자체를 하지 않는다.
	 *
	 * [이건 표시 규칙이지 판정이 아니다] 프롬프트가 사라져도 E 키는 그대로 먹는다 —
	 * 어빌리티(UGAB_Interact)는 페이즈를 보지 않기 때문이다. 준비 시간에 물건을
	 * 정말로 못 집게 하려면 그쪽에서 막아야 하고, 그건 UI 가 할 일이 아니다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Interact")
	bool IsPromptAllowed() const { return bPromptAllowed; }

protected:
	//~ UUserWidget
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──

	/** 행동 문구 ("[E] 집기") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Interact", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Action;

	/** 상세 문구 ("$8,000 · 2인 필요"). 없어도 동작한다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Interact", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Detail;

	// ── 시선 스윕 설정 ──

	/**
	 * 스윕 거리.
	 *
	 * UGAB_Interact::InteractionRange 와 같은 값이어야 한다. 어긋나면
	 * "프롬프트는 떴는데 E 를 눌러도 안 잡히는" 거짓말이 된다.
	 * 두 값을 한 곳에서 읽게 고치는 것은 플레이어 파트와 합의가 필요하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact Prompt", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRange = 300.f;

	/** 스윕 반지름. UGAB_Interact::InteractionRadius 와 같은 값이어야 한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact Prompt", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 30.f;

	/**
	 * 조준을 다시 확인하는 주기(초).
	 *
	 * 매 프레임 쏠 이유가 없다. 0.1초면 시선을 옮긴 것이 늦다고 느껴지지 않으면서
	 * 스윕이 프레임당 1회에서 초당 10회로 줄어든다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact Prompt", meta = (ClampMin = "0.01", Units = "s"))
	float RefreshInterval = 0.1f;

	/**
	 * 물건을 들고 있을 때도 프롬프트를 띄울 것인가.
	 *
	 * [기본이 거짓인 이유] HUD 우하단 소지 슬롯이 이름 · 무게 · 가치를 이미 보여준다.
	 *   화면 중앙에 같은 이름을 한 번 더 띄우면 시선만 분산되고, 조준선 자리에
	 *   글자가 상시로 남아 정작 다음 목표를 보는 데 방해가 된다.
	 *
	 * [끄면 같이 사라지는 것] [Q] 놓기 · [좌클릭] 던지기 안내도 함께 없어진다.
	 *   조작을 처음 익히는 자리(튜토리얼 · 시연용 맵)에서는 켜 둘 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact Prompt")
	bool bShowHoldingPrompt = false;

	// ── BP 연출 훅 ──
	//
	// 표시는 C++ 이 이미 끝냈다. 여기는 사운드 · 페이드 자리다. 게임 상태를 바꾸지 말 것.

	/** 프롬프트가 새로 떴거나 문구가 바뀌었다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Interact")
	void OnPromptShown(const FText& Action);

	/** 프롬프트가 사라졌다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Interact")
	void OnPromptHidden();

private:
	/** 주기 콜백. 조준 대상을 다시 찾아 문구를 갱신한다 */
	void RefreshFocus();

	/**
	 * 페이즈 구독을 건다. GameState 가 아직 없으면 도착할 때 다시 불린다.
	 *
	 * 재시도 타이머 대신 UWorld::GameStateSetEvent 를 쓴다 —
	 * 엔진이 GameState 세팅 순간을 알려주므로 대기 간격이라는 상수가 필요 없다.
	 */
	void BindToHeistGameState();

	/** UWorld::GameStateSetEvent 콜백. 작업 레벨의 GameState 면 구독을 건다 */
	void HandleGameStateSet(AGameStateBase* NewGameState);

	UFUNCTION()
	void HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason);

	/** 이 페이즈에서 프롬프트를 띄울지 정한다 */
	void ApplyPhaseGate(FGameplayTag Phase);

	/** 허용 여부를 바꾼다. 꺼지면 프롬프트도 같이 내린다 */
	void SetPromptAllowed(bool bAllowed, const TCHAR* Cause);

	/** 조준 대상이 정해진 뒤 문구를 결정한다. 대상이 없으면 숨긴다 */
	void DecidePrompt(AActor* Target, const APawn* Pawn);

	/** 이미 물건을 들고 있을 때의 문구 */
	void ShowHoldingPrompt(AActor* Held);

	void ShowPrompt(const FText& Action, const FText& Detail);
	void HidePrompt();

	FTimerHandle RefreshHandle;

	/** 마지막으로 쓴 문구. 같으면 SetText 를 건너뛴다 */
	FText LastAction;
	FText LastDetail;

	/**
	 * 마지막으로 문구를 정하거나 숨긴 사유. `hh.UI.PromptDebug` 전용이다.
	 *
	 * 프롬프트는 대상이 없으면 사라지는 것이 정상 동작이라, 화면만 봐서는
	 * "안 붙었다" 와 "못 찾았다" 를 구분할 수 없다. 그 둘을 갈라주는 값이다.
	 */
	FString DebugReason;

	UPROPERTY()
	TObjectPtr<AHeistGameState> BoundState;

	/** GameStateSetEvent 구독 해제용. 다이나믹 델리게이트가 아니라 핸들로 뗀다 */
	FDelegateHandle GameStateSetHandle;

	bool bPromptVisible = false;

	/**
	 * 지금 페이즈에서 프롬프트가 허용되는가.
	 *
	 * 기본값이 true 인 것은 의도다 — 작업 레벨이 아닌 맵(L_NoiseTest · GuardTest)에는
	 * 페이즈가 없어서 이 값을 갱신할 사람이 없다. 기본이 false 면 그 맵들에서
	 * 프롬프트가 영영 안 뜨고, 프롬프트는 원래 안 뜨는 것이 정상인 UI 라 아무도 못 알아챈다.
	 */
	bool bPromptAllowed = true;
};
