#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/HeistOutcome.h"         // EHeistOutcome — UFUNCTION 반환 타입이라 전방 선언 불가
#include "HeistResultWidget.generated.h"

class AGameStateBase;
class AHeistGameState;
class UButton;
class UPanelWidget;
class UResultRowWidget;
class UTextBlock;
class UWidgetAnimation;

/**
 * 결과 화면 (기획서 8장, 시안 ui_result.png). WBP_Result 의 C++ 베이스다.
 *
 * [언제 뜨나] AHeavyHUD 가 Phase.Result 에서 만들어 인게임 HUD 위에 얹는다.
 *   위젯 자신은 언제 뜨는지 모른다 — 만들어진 시점이 곧 결과 시점이다.
 *
 * [데이터는 전부 AHeistGameState 에서 온다] 여기서 계산하는 것은 없다.
 *   등급 · 소요 시간 · 적재 목록 · 기여도 · 최다 소음 유발자가 이미 서버에서 확정돼
 *   복제로 도착해 있다. UI 는 읽고 줄 세우고 문구로 옮기기만 한다 (아키텍처 규칙 5).
 *
 * [값이 한 번 정해지면 안 바뀐다] 결과는 Result 진입 순간 고정된다. 그래서 대부분을
 *   바인딩 시점에 한 번만 그린다. 계속 움직이는 것은 두 개뿐이다 —
 *   남은 체류 시간과 확인 인원.
 *
 * [목록은 지금 여러 줄 텍스트다] 시안의 아이템 카드처럼 만들려면 항목 위젯 클래스가
 *   따로 필요하고, 아이콘도 아직 없다. 데이터를 꺼내 줄 세우는 부분은 그대로 두고
 *   그리는 쪽만 나중에 교체할 수 있게 문자열 조립을 한 함수에 모아 두었다.
 *
 * [C++ 과 WBP 의 경계] 문구 · 색 · 가시성 · 버튼 활성화는 C++ 이 정한다.
 *   WBP 는 배치와 애니메이션 저작만 한다. 아래 BindWidget 이름이 그 창구다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API UHeistResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 판의 결과 등급. 못 붙었으면 Failure */
	UFUNCTION(BlueprintPure, Category = "UI|Result")
	EHeistOutcome GetOutcome() const;

	/** 등급 문구 ("탈출 성공!" · "부분 성공" · "작전 실패") */
	UFUNCTION(BlueprintPure, Category = "UI|Result")
	FText GetOutcomeText() const;

	/** 등급 색. 성공은 금색, 실패는 경보색 */
	UFUNCTION(BlueprintPure, Category = "UI|Result")
	FLinearColor GetOutcomeColor() const;

	/** 이 화면을 이미 확인했는가. 버튼을 두 번 누르지 못하게 막는 값 */
	UFUNCTION(BlueprintPure, Category = "UI|Result")
	bool IsLocalPlayerConfirmed() const { return bLocalConfirmed; }

protected:
	//~ UUserWidget
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──
	//
	// Txt_Outcome 만 필수다. 나머지는 없어도 화면이 성립하므로 Optional 이고,
	// 없으면 NativeConstruct 에서 로그로 알린다 — 이름을 틀렸을 때
	// "그 칸만 안 채워지는" 조용한 실패를 눈으로는 구별할 수 없기 때문이다.

	/** 큰 제목 ("탈출 성공!") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Outcome;

	/** 부제 ("수집한 전리품을 챙기고 돌아왔다") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Subtitle;

	/** 플레이 시간 ("6:52") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Elapsed;

	/** 획득 금액 ("$3,750 / $50,000") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Money;

	/** 실어온 노획물 개수 ("4개") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_LootCount;

	/**
	 * 최다 소음 유발자 (기획서 8장 — "반드시 넣는다").
	 *
	 * 결과 화면에서 책임 소재가 드러나는 것이 이 게임의 코미디를 완성한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Noisiest;

	/** 적재 목록. 한 줄에 한 종류 ("도자기 세트 x2    $5,800") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_LootList;

	/** 플레이어별 결과. 한 줄에 한 명 ("Player01    $1,290    탈출") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_PlayerList;

	/** 확인 상태 ("로비로 돌아가기 (15)" · "2/4 확인") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Confirm;

	/** 확인 버튼. 누르면 서버에 알리고 비활성화된다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Confirm;

	/** 목표 대비 달성률 ("목표 $50,000 대비 106%") */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TargetRatio;

	/**
	 * 최다 소음 유발자 부연 ("경계도 42% 유발 — 이 작업 최다").
	 *
	 * [횟수가 아니라 기여량이다] 시안에는 "소음 발생 14회" 로 되어 있지만
	 *   집계되는 값은 누적 경계도 기여량(%)이고 발생 횟수는 아무도 세지 않는다.
	 *   횟수가 필요하면 소음 · 경계도 파트에 새로 요청해야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_NoisiestDetail;

	// ── 목록을 행 위젯으로 그릴 때 ──
	//
	// 컨테이너와 행 클래스가 둘 다 있으면 행 위젯으로 그리고, 하나라도 없으면
	// 기존 여러 줄 텍스트(Txt_LootList · Txt_PlayerList)로 떨어진다.
	// 둘을 같이 두는 것은 WBP 를 한 번에 갈아엎지 않아도 되게 하기 위해서다.

	/** 적재 목록 행이 들어갈 컨테이너 (세로 박스 등) */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_LootList;

	/** 개인 기여도 행이 들어갈 컨테이너 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_PlayerList;

	/** 적재 목록 한 줄을 그릴 위젯. 보통 바가 없는 WBP 를 꽂는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	TSubclassOf<UResultRowWidget> LootRowClass;

	/** 개인 기여도 한 줄을 그릴 위젯. 바가 있는 WBP 를 꽂는다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Result")
	TSubclassOf<UResultRowWidget> PlayerRowClass;

	/** 등장 연출. 없으면 그냥 즉시 뜬다 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Result", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Intro;

	// ── BP 연출 훅 ──
	//
	// 필수 표시는 C++ 이 이미 끝냈다. 여기는 사운드 · 파티클 자리다.
	// 게임 상태를 바꾸지 말 것 — 훅은 클라이언트에서도 돈다.

	/** 결과가 채워졌다. 등급에 따라 다른 연출을 걸 자리 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Result")
	void OnResultShown(EHeistOutcome Outcome);

	/** 확인 인원이 바뀌었다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Result")
	void OnConfirmCountChanged(int32 NumConfirmed, int32 NumPlayers);

	/** 이 플레이어가 확인을 눌렀다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Result")
	void OnLocalConfirmed();

private:
	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleConfirmCountChanged(int32 NumConfirmed, int32 NumPlayers);

	/** 페이즈와 달리 결과는 이미 도착해 있을 확률이 높지만, 클라에서는 늦을 수 있다 */
	void BindToHeistGameState();

	/** UWorld::GameStateSetEvent 콜백 */
	void HandleGameStateSet(AGameStateBase* NewGameState);

	/** 한 번만 그리는 것들 — 등급 · 시간 · 금액 · 목록 · 최다 소음 유발자 */
	void PopulateResult();

	/** 적재 목록 문자열을 만든다. 같은 종류는 묶고 비싼 것부터 */
	FText BuildLootList() const;

	/** 플레이어 목록 문자열을 만든다. 많이 벌어온 사람부터 */
	FText BuildPlayerList() const;

	/** 적재 목록을 행 위젯으로 채운다. 컨테이너나 행 클래스가 없으면 아무것도 하지 않는다 */
	void PopulateLootRows();

	/** 개인 기여도를 행 위젯으로 채운다. 바 비율은 1등 기준이 아니라 총액 기준이다 */
	void PopulatePlayerRows();

	/** 주기 콜백. 남은 체류 시간을 다시 계산해 버튼 문구에 반영한다 */
	void RefreshCountdown();

	/** 버튼 문구와 활성화 상태를 한꺼번에 정한다 */
	void ApplyConfirmVisual();

	UPROPERTY()
	TObjectPtr<AHeistGameState> BoundState;

	FDelegateHandle GameStateSetHandle;
	FTimerHandle CountdownHandle;

	/** 마지막으로 버튼에 쓴 정수 초. 같으면 SetText 를 건너뛴다 */
	int32 LastShownSeconds = INDEX_NONE;

	// 이름이 위 BP 훅(OnConfirmCountChanged)의 파라미터와 겹치면 안 된다 —
	// UHT 가 만드는 코드에서 지역 변수가 멤버를 가려 C4458 이 뜨고, 이 프로젝트는
	// 경고를 에러로 다룬다. BP 에 보이는 파라미터 이름이 더 중요하므로 멤버 쪽을 비켜 준다
	int32 ConfirmedCount = 0;
	int32 PlayerCount = 0;

	/** 이 화면에서 확인을 눌렀는가. 서버 응답을 기다리지 않고 바로 잠근다 */
	bool bLocalConfirmed = false;

	/** 남은 시간 갱신 주기(초). 초 단위 표시라 0.1초면 넘어가는 순간을 놓치지 않는다 */
	static constexpr float CountdownInterval = 0.1f;
};
