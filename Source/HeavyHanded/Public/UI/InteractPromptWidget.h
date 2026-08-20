#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractPromptWidget.generated.h"

class AActor;
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

	bool bPromptVisible = false;
};
