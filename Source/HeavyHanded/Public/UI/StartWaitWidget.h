#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartWaitWidget.generated.h"

class UBackgroundBlur;
class UImage;
class UPanelWidget;
class UTextBlock;

/**
 * 접속 대기 오버레이 (기획서 8장 UI · 코어 루프 "접속 대기 — Prep 이전").
 *
 * 리슨 서버라 호스트는 레벨이 열리는 즉시 들어와 있고 클라이언트는 로딩이 늦다.
 * 전원이 모일 때까지 화면 하단에 "2 / 4 함께하는 중" 과 인원 수만큼 채워지는 바를 띄운다.
 *
 * [로딩 화면과 다른 위젯이다] 1부(ULoadingScreenWidget)는 화면 전체를 덮는 정적 화면이고
 *   이쪽은 배경이 비치는 하단 최소 UI 다. 한 클래스로 겸하면 레이아웃이 서로를 망친다.
 *
 * [입력은 이 위젯이 막지 않는다] 대기 중 입력 차단은 AHeistPlayerController 가 이미 한다.
 *   여기서 또 막으면 두 곳이 서로 모른 채 커서를 켰다 껐다 하게 된다.
 *   그래서 이 위젯은 항상 클릭을 통과시킨다(SelfHitTestInvisible).
 *
 * [숫자를 직접 조회하지 않는다] 대기 상태는 서버가 세어 복제해 주는 값이라
 *   여기서 GetNumPlayers() 같은 것을 세면 클라이언트마다 다른 숫자가 나온다.
 *   UpdateWaitState() 로 받기만 한다 — UI 는 읽고 구독만 한다.
 *
 * WBP 가 만들 것 — 아래 BindWidgetOptional 과 똑같은 이름의 위젯들.
 * 색 · 블러 강도는 찍지 않는다. 전부 C++ 이 PreConstruct 에서 UUISettings 토큰으로 칠한다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UStartWaitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대기 상태를 꽂는다. 값이 바뀔 때마다 부른다.
	 *
	 * @param bInWaiting      아직 기다리는 중인가. false 면 위젯이 스스로 접힌다
	 * @param InNumConnected  지금까지 들어온 인원
	 * @param InNumExpected   이 판에 올 인원. **0 이면 "모른다"는 뜻이다** —
	 *                        그때는 "3/0" 대신 인원 없는 문구가 나가고 바가 통째로 숨는다
	 *
	 * [파라미터가 구조체가 아닌 이유] AHeistPlayerController 의
	 * OnStartWaitChanged(bool, int32, int32) 훅과 인자를 그대로 맞춰 뒀다.
	 * 코어 루프 쪽 헤더를 하나도 물지 않아서 이 위젯만 따로 만들고 시험할 수 있다
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|StartWait")
	void UpdateWaitState(bool bInWaiting, int32 InNumConnected, int32 InNumExpected);

	/** 지금 대기 중인가 */
	UFUNCTION(BlueprintPure, Category = "UI|StartWait")
	bool IsWaiting() const { return bWaiting; }

protected:
	virtual void NativePreConstruct() override;

	/** 색 · 폰트 · 블러 강도를 토큰에서 칠한다. 디자이너에서도 돈다 */
	void ApplyTokens();

	/** 문구와 바를 지금 값으로 다시 그린다 */
	void Refresh(int32 Connected, int32 Expected);

	// ── WBP 가 배치하는 위젯들 ──
	//
	// 전부 Optional 이다. 하나를 빼도 화면이 죽지 않고 그 자리만 비는 쪽이,
	// 디자인을 바꿀 때마다 C++ 이 터지는 것보다 낫다.
	//
	// [이름을 그대로 쓸 것] WBP 에 이미 같은 이름의 변수가 있으면 바인딩이 조용히
	// 어긋나 포인터가 쓰레기가 된다. 새로 만드는 위젯에 아래 이름을 붙인다

	/** "2 / 4 함께하는 중" */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Status;

	/**
	 * 바를 담는 가로 박스. **바를 하나씩 바인드하지 않는다.**
	 *
	 * 자식을 순회하며 색과 표시를 칠하므로, WBP 에서 바 개수를 2개로 줄이든 6개로 늘리든
	 * C++ 을 고치지 않는다. 자식은 Image 로 둘 것 — 다른 타입이면 색은 못 칠하고
	 * 남는 칸을 접는 것만 동작한다
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_Slots;

	/** 배경 블러. 강도는 설정값이 들어간다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBackgroundBlur> Blur_Bg;

	/** 화면을 약하게 어둡게 덮는 판. 배경색에 투명도만 얹어 칠한다 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Dim;

	/**
	 * 디자이너에서만 쓰는 미리보기 인원.
	 *
	 * 실행 중에는 UpdateWaitState() 가 덮으므로 게임에 영향이 없다.
	 * 이게 없으면 UMG 편집기에서 바가 전부 회색이라 채워진 색을 눈으로 맞출 수가 없다
	 */
	UPROPERTY(EditAnywhere, Category = "StartWait|Preview", meta = (ClampMin = "0"))
	int32 PreviewConnected = 2;

	/** 디자이너 미리보기 예정 인원. 0 으로 두면 "인원 모름" 화면을 미리 볼 수 있다 */
	UPROPERTY(EditAnywhere, Category = "StartWait|Preview", meta = (ClampMin = "0"))
	int32 PreviewExpected = 4;

private:
	/** 마지막으로 받은 대기 상태. PreConstruct 가 다시 돌아도 그대로 복원된다 */
	bool bWaiting = false;

	int32 NumConnected = 0;

	int32 NumExpected = 0;
};
