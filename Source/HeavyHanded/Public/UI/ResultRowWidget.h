#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultRowWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 결과 화면의 목록 한 줄 (시안 sneakers_result_screen — 적재 목록 · 개인 기여도).
 *
 * [한 클래스로 두 목록을 다 그린다] 적재 목록은 "이름 ─ 금액", 기여도는
 *   "이름 ─ 바 ─ 퍼센트" 다. 모양이 다른 것은 바 하나뿐이고 그것도 선택 항목이라,
 *   클래스를 둘로 나누면 같은 코드가 두 벌이 된다. 적재 행 WBP 는 Bar_Share 를
 *   배치하지 않으면 그만이다.
 *
 * [왜 여러 줄 텍스트로는 안 되나] 금액을 오른쪽 끝에 맞추려면 줄마다 폭을 알아야 하고,
 *   기여도 바는 애초에 글자가 아니다. 공백으로 맞추면 폰트가 바뀌는 순간 어긋난다.
 *
 * [값을 스스로 찾지 않는다] UHeistResultWidget 이 집계를 끝내고 SetRow 로 떠먹여 준다.
 *   행이 각자 GameState 를 뒤지면 정렬 · 묶기 규칙이 행마다 흩어진다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UResultRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 한 줄을 채운다.
	 *
	 * @param Label   왼쪽 문구 (노획물 이름 · 플레이어 이름)
	 * @param Amount  오른쪽 문구 (금액 · 퍼센트)
	 * @param Ratio   바 채움 0~1. 음수면 바를 숨긴다 — 적재 목록에는 바가 없다
	 * @param bDimmed 흐리게 그릴 것인가. 파손 · 유출로 가치가 깎인 항목에 쓴다
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Result")
	void SetRow(const FText& Label, const FText& Amount, float Ratio, bool bDimmed);

protected:
	//~ UUserWidget
	virtual void NativePreConstruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──
	//
	// 셋 다 Optional 이다. 적재 행에는 Bar_Share 가 없고,
	// 디자인에 따라 금액을 안 쓰는 행도 있을 수 있다.

	/** 왼쪽 문구 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Label;

	/** 오른쪽 문구 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Amount;

	/** 기여도 바. 적재 행 WBP 에는 배치하지 않는다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar_Share;

	/**
	 * 흐리게 그릴 때의 불투명도.
	 *
	 * 색을 따로 만들지 않고 알파만 낮춘다 — 파손된 항목을 위한 토큰을 새로 만들면
	 * 팔레트가 늘어나고, "깎였다" 는 사실은 밝기만 낮춰도 충분히 읽힌다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result Row", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimmedOpacity = 0.45f;

	// ── BP 연출 훅 ──

	/** 값이 채워졌다. 등장 애니메이션 자리 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Result")
	void OnRowSet(bool bIsDimmed);
};
