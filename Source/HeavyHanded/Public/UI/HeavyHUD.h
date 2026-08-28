#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Templates/SubclassOf.h"       // HUDWidgetClass 를 값으로 보유 — 전방 선언 불가
#include "GameplayTagContainer.h"       // FGameplayTag — UFUNCTION 파라미터라 전방 선언 불가
#include "Core/HeistPhase.h"            // EHeistPhaseReason — 델리게이트 콜백 시그니처에 들어간다
#include "HeavyHUD.generated.h"

class AGameStateBase;
class AHeistGameState;
class UUserWidget;

/**
 * 인게임 HUD 의 소유자 (기획서 8장).
 *
 * [왜 AHUD 인가] 위젯을 화면에 올리는 코드가 프로젝트에 한 곳도 없었고, 그래서 만들어 둔
 *   WBP_HUD 가 어느 맵에서도 뜨지 않았다. 레벨 블루프린트에 넣으면 맵마다 다시 넣어야 하고
 *   빠뜨린 맵에서는 조용히 HUD 가 없는 상태가 된다. AHUD 는 엔진이 플레이어마다 자동으로
 *   스폰하므로 GameMode 의 HUDClass 한 번으로 전 맵에 적용된다.
 *
 * [소유 범위] AHUD 는 로컬 플레이어를 가진 컨트롤러에만 스폰된다. 전용 서버에는 생기지 않고,
 *   리슨 서버 호스트는 자기 것 하나만 갖는다. 그래서 여기서 권위를 따질 필요가 없다 —
 *   HUD 는 판정하지 않고 복제된 상태를 읽기만 한다 (아키텍처 규칙 5).
 *
 * [페이즈에 따라 화면을 얹는다] Phase.Result 가 되면 결과 화면을 HUD 위에 올린다.
 *   이 판단을 여기 두는 이유는 "지금 어느 화면인가" 를 아는 주체가 화면 하나가 아니라
 *   화면들의 소유자여야 하기 때문이다. 위젯이 서로를 숨기게 만들면 위젯끼리 참조가 생기고,
 *   하나가 없는 맵에서 조용히 깨진다.
 *
 * [인게임 HUD 는 페이즈로 숨기지 않는다] 결과 화면에서도 HUD 는 그대로 떠 있다.
 *   결과 화면이 HUD 를 덮는 전면 화면이 아니라 그 위에 얹히는 패널이기 때문이다.
 *   HUD 를 숨기는 경우는 컷신 하나뿐이고, 그건 페이즈가 아니라 시퀀스 재생이 정하므로
 *   SetHUDVisible() 을 컷신 쪽에서 부른다.
 *
 * [작업 레벨이 아닐 때] AHeistGameState 가 없는 맵(L_UITest · GuardTest)에서는
 *   페이즈 자체가 없으므로 아무것도 전환하지 않는다. HUD 는 그대로 떠 있다.
 *
 * [C++ 과 BP 의 경계] 언제 만들고 언제 붙이고 떼는지는 C++ 이 정한다.
 *   BP 서브클래스가 정하는 것은 어떤 WBP 를 꽂을지(HUDWidgetClass · ResultWidgetClass)뿐이다.
 *
 * [Abstract 가 아닌 이유] 위젯 베이스와 달리 이 클래스는 그 자체로 동작한다.
 *   BP 서브클래스를 강제하는 대신, HUDWidgetClass 가 비어 있으면 로그로 알린다 —
 *   HUD 가 안 뜨는 것은 화면만 봐서는 원인을 알 수 없는 대표적인 침묵이다.
 */
UCLASS()
class HEAVYHANDED_API AHeavyHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHeavyHUD();

	/** 만들어진 HUD 위젯. 아직 안 만들어졌으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	UUserWidget* GetHUDWidget() const { return HUDWidget; }

	/**
	 * 인게임 HUD 를 보이거나 숨긴다.
	 *
	 * 떼었다 다시 붙이지 않고 가시성만 바꾼다 — 위젯을 떼면 구독이 풀려서
	 * 다시 붙였을 때 게이지가 0 부터 시작한다.
	 *
	 * [부르는 곳은 컷신뿐이다] 페이즈로는 숨기지 않는다 (클래스 주석 참고).
	 *   컷신이 시작할 때 false, 끝날 때 true 로 되돌리는 짝으로만 쓸 것 —
	 *   false 로 두고 되돌리지 않으면 HUD 가 영영 사라지고, 그 상태는
	 *   화면만 봐서는 "아직 안 만들었나" 와 구별되지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void SetHUDVisible(bool bVisible);

	/** HUD 가 지금 보이는 상태인가 */
	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	bool IsHUDVisible() const;

	/** 떠 있는 결과 화면. Phase.Result 가 아니면 nullptr */
	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	UUserWidget* GetResultWidget() const { return ResultWidget; }

protected:
	//~ AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End

	/**
	 * 인게임 HUD 위젯 클래스. BP 서브클래스에서 WBP_HUD 를 지정한다.
	 *
	 * 비워 두면 HUD 가 아예 뜨지 않는다. 그 상태는 화면만 봐서는 "아직 안 만들었나?" 로만
	 * 보이므로 BeginPlay 에서 경고를 남긴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HUD")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	/**
	 * HUD 레이어 순서. 값이 클수록 위에 그려진다.
	 *
	 * 결과 화면 · 상점처럼 HUD 를 덮어야 하는 화면이 나중에 붙으므로,
	 * 인게임 HUD 는 바닥(0)에 두고 그 위를 비워 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HUD")
	int32 HUDZOrder = 0;

	/**
	 * 결과 화면 위젯 클래스. BP 서브클래스에서 WBP_Result 를 지정한다.
	 *
	 * 비어 있으면 Phase.Result 에서 HUD 만 사라지고 아무것도 뜨지 않는다.
	 * 결과 화면이 아직 없는 지금은 그것이 정상이라 경고가 아니라 Log 로 남긴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HUD")
	TSubclassOf<UUserWidget> ResultWidgetClass;

	/**
	 * 결과 화면 레이어 순서. 인게임 HUD(HUDZOrder = 0) 위에 올라와야 한다.
	 *
	 * 결과 페이즈에도 HUD 는 떠 있으므로 둘은 실제로 겹친다 — 이 값이 그 순서를 정한다.
	 * HUD 보다 작게 두면 결과 화면이 HUD 뒤로 들어가 가려진다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HUD")
	int32 ResultZOrder = 10;

private:
	/** HUD 위젯을 만들어 화면에 붙인다. 실패하면 이유를 로그로 남긴다 */
	void CreateAndAddHUDWidget();

	/**
	 * 페이즈 구독을 건다. GameState 가 아직 없으면 도착할 때 다시 불린다.
	 *
	 * [왜 재시도 타이머가 아닌가] 클라이언트에서 GameState 는 복제로 뒤늦게 온다.
	 *   UHeistHUDWidget 은 0.25초 폴링으로 기다리지만, 엔진이 그 순간을
	 *   UWorld::GameStateSetEvent 로 이미 알려준다. 이벤트를 쓰면 대기 간격과
	 *   포기 시각이라는 상수 두 개가 아예 필요 없어진다.
	 */
	void BindToHeistGameState();

	/** UWorld::GameStateSetEvent 콜백. 작업 레벨의 GameState 면 구독을 건다 */
	void HandleGameStateSet(AGameStateBase* NewGameState);

	UFUNCTION()
	void HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason);

	/** 이 페이즈에 맞는 화면을 띄운다. 페이즈 판단은 이 함수 하나에만 있다 */
	void ApplyPhaseLayer(FGameplayTag Phase);

	/** 결과 화면을 만들어 올린다. 이미 떠 있으면 아무것도 하지 않는다 */
	void ShowResultWidget();

	/**
	 * 결과 화면을 뗀다.
	 *
	 * HUD 와 달리 가시성이 아니라 실제로 제거한다 — 결과 화면은 판이 끝난 뒤
	 * 한 번 뜨고 마는 화면이라 구독을 유지할 이유가 없고, 다음 판은 레벨이 새로 열린다.
	 */
	void RemoveResultWidget();

	UPROPERTY()
	TObjectPtr<UUserWidget> HUDWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> ResultWidget;

	UPROPERTY()
	TObjectPtr<AHeistGameState> BoundState;

	/** GameStateSetEvent 구독 해제용. 다이나믹 델리게이트가 아니라 핸들로 뗀다 */
	FDelegateHandle GameStateSetHandle;

	/** ResultWidgetClass 가 비었다는 로그를 한 번만 남기기 위한 플래그 */
	bool bWarnedNoResultClass = false;
};
