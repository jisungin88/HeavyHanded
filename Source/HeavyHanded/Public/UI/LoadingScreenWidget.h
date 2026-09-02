#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LoadingTypes.h"   // FLoadingScreenContent — UFUNCTION 파라미터라 전방 선언이 불가능하다
#include "LoadingScreenWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * 로딩 화면 (기획서 8장 UI).
 *
 * [진행률 바가 없다] 논심리스 트래블에서는 클라이언트가 LoadMap 안에서 통째로 멈춘다.
 *   그 동안 게임 스레드가 돌지 않으므로 진행률을 물어볼 상대가 없고, 엔진도
 *   블로킹 로드에는 진행률 API 를 주지 않는다(GetAsyncLoadPercentage 는 -1 이다).
 *   숫자를 띄우려면 시간으로 지어내는 수밖에 없어서 바 자체를 두지 않기로 했다.
 *   실제 숫자는 로딩이 끝난 뒤 접속 대기(2부)에서 "2/4" 로 나온다.
 *
 * [그래서 이 위젯은 정적이다] 애니메이션도 바인딩도 없다. 만들 때 문구를 받고,
 *   그 뒤로는 SetStatusText() 로 하단 한 줄만 바뀐다. 1부에서는 그것도 바뀌지 않는다.
 *
 * [1부와 2부가 같은 클래스다] 로딩(MoviePlayer)과 접속 대기(뷰포트 위젯)는 띄우는
 *   방법이 다르지만 화면은 같아야 한다. 다른 위젯 두 개로 만들면 반드시 어긋난다.
 *
 * WBP 가 만들 것 — 아래 BindWidgetOptional 이름과 똑같은 이름의 텍스트 블록.
 * 색과 폰트는 찍지 않는다. 전부 C++ 이 PreConstruct 에서 UUISettings 토큰으로 칠한다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 문구 한 벌을 꽂는다. 만든 직후 한 번 부른다.
	 *
	 * 뷰포트에 올리기 전에 불러도 된다 — BindWidget 포인터는 CreateWidget 안에서 이미 채워진다
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void ApplyContent(const FLoadingScreenContent& NewContent);

	/** 하단 상태 문구만 바꾼다. 접속 대기에서 인원이 늘 때마다 부른다 */
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void SetStatusText(const FText& NewStatus);

	/** 지금 꽂혀 있는 문구. UFUNCTION 은 참조를 돌려줄 수 없어 값 복사다 */
	UFUNCTION(BlueprintPure, Category = "UI|Loading")
	FLoadingScreenContent GetContent() const { return Content; }

protected:
	virtual void NativePreConstruct() override;

	/** 색과 폰트를 토큰에서 칠한다. 디자이너에서도 돌기 때문에 편집 중에도 그대로 보인다 */
	void ApplyTokens();

	/** 문구를 실제 텍스트 블록에 꽂는다 */
	void ApplyContentTo(const FLoadingScreenContent& Source);

	// ── WBP 가 배치하는 위젯들 ──
	//
	// 전부 Optional 이다. 하나를 빼도 화면이 죽지 않고 그 줄만 비는 쪽이,
	// 디자인을 바꿀 때마다 C++ 이 터지는 것보다 낫다.
	//
	// [이름을 그대로 쓸 것] WBP 에 이미 같은 이름의 변수가 있으면 바인딩이 조용히
	// 어긋난다. 새로 만드는 텍스트 블록에 아래 이름을 붙인다

	/** "다음 작업" */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Eyebrow;

	/** "박물관 진입 중…" */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Title;

	/** "TIP" */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TipLabel;

	/** 팁 본문 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Tip;

	/** "레벨 로딩 중…" / "동료를 기다리는 중 2/4" */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Status;

	/** 화면 전체를 덮는 바탕. 배경색을 여기에 칠한다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Background;

	/**
	 * 디자이너에서만 쓰는 미리보기 문구.
	 *
	 * 실행 중에는 ApplyContent() 가 덮으므로 게임에 영향이 없다.
	 * 이게 없으면 UMG 편집기에서 빈 화면만 보여 글자 크기를 맞출 수가 없다
	 */
	UPROPERTY(EditAnywhere, Category = "Loading|Preview")
	FLoadingScreenContent PreviewContent;

private:
	/** 지금 꽂혀 있는 문구. PreConstruct 가 다시 돌아도 그대로 복원된다 */
	UPROPERTY()
	FLoadingScreenContent Content;
};
