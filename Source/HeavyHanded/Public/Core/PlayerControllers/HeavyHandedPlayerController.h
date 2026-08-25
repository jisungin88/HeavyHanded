#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"        // HUDWidgetClass 를 값으로 보유 — 전방 선언 불가
#include "HeavyHandedPlayerController.generated.h"

class UUserWidget;

/**
 * UI 를 조작하는 동안 게임 입력을 어떻게 할 것인가.
 *
 * 두 가지로 나눈 이유는 은신처와 결과 화면의 요구가 정반대이기 때문이다.
 * 은신처 단말기는 조작 중에도 주변이 살아 있어야 하고(옆에서 팀원이 걸어 다니는 것이 보인다),
 * 결과 화면은 판이 이미 끝났으므로 게임 입력이 들어가면 안 된다.
 */
UENUM(BlueprintType)
enum class EHHUIFocusMode : uint8
{
	/** 게임 입력을 살려 둔다 — 은신처 상점 · 작업대 · 지도 */
	GameAndUI   UMETA(DisplayName = "Game And UI"),

	/** 게임 입력을 막는다 — 결과 화면 */
	UIOnly      UMETA(DisplayName = "UI Only")
};

/**
 * 은신처와 작업 레벨의 PlayerController 가 공유하는 베이스.
 *
 * [무엇이 여기 있고 무엇이 없는가]
 *   두 레벨이 **실제로 둘 다 쓰는 것**만 둔다 — HUD 를 만들어 붙이는 절차와, UI 조작 중의
 *   입력 모드. 채팅은 아직 AShelterPlayerController 에만 있고, 작업 중에도 쓰기로 확정되는
 *   시점에 여기로 올라온다. 그 이전에 미리 올려 두면 쓰지 않는 RPC 가 작업 레벨에 생긴다.
 *
 * [위젯이 아니라 절차만 공유한다]
 *   은신처와 작업 레벨의 HUD 는 겹치는 항목이 거의 없다 (기획서 8장). 그래서 공유하는 것은
 *   "만들어 붙이고 뗀다" 는 절차 하나이고, 무엇을 띄울지는 각 BP 가 값으로 정한다.
 *
 * [ATitlePlayerController 는 여기서 파생하지 않는다]
 *   타이틀에는 HUD 도 월드도 없다. 지금 공유할 코드가 하나도 없는데 상속시키면
 *   쓰지 않는 API 를 물려받기만 한다.
 *
 * [Abstract 인 이유]
 *   이 클래스를 GameMode 의 PlayerControllerClass 로 지정하는 것은 언제나 실수다.
 *   Abstract 면 스폰이 실패해 그 자리에서 드러난다 — 지정된 채로 조용히 도는 것보다 낫다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API AHeavyHandedPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// ──────────────────────────────────────────────
	// HUD
	// ──────────────────────────────────────────────

	/**
	 * HUD 를 만들어 화면에 붙인다. 이미 있으면 그대로 돌려준다.
	 *
	 * 언제 부를지는 파생 클래스가 정한다 — 은신처는 접속하자마자가 자연스럽고,
	 * 작업 레벨은 페이즈가 시작될 때다.
	 *
	 * @return  붙은 위젯. 로컬 컨트롤러가 아니거나 클래스가 비어 있으면 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|UI")
	UUserWidget* EnsureHUDWidget();

	/** HUD 를 떼고 버린다. 없으면 아무것도 하지 않는다 */
	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|UI")
	void RemoveHUDWidget();

	UFUNCTION(BlueprintPure, Category = "HeavyHanded|UI")
	UUserWidget* GetHUDWidget() const { return HUDWidget; }

	// ──────────────────────────────────────────────
	// 입력 포커스
	// ──────────────────────────────────────────────

	/**
	 * UI 조작 모드로 들어간다. 커서를 켜고 입력을 UI 로 보낸다.
	 *
	 * [왜 소유자를 받는가] SetInputMode 를 각자 부르면 반드시 한 곳이 복구를 빠뜨린다.
	 *   증상은 "커서가 안 사라진다" · "캐릭터가 안 움직인다" 로 나타나는데, 그때 화면에는
	 *   아무 UI 도 안 떠 있어서 **누가 안 놓았는지 알 수 없다.** 은신처가 디제틱 UI 로 가면
	 *   상점 · 작업대 · 지도 · 게시판이 전부 이 경로를 타므로 그만큼 더 자주 난다.
	 *   소유자를 기록해 두면 남의 포커스를 실수로 지울 수 없고, 로그에 누가 쥐고 있는지 남는다.
	 *
	 * 이미 다른 객체가 쥐고 있으면 **경고를 남기고 넘겨받는다.** 거절하지 않는 이유는
	 * 거절하면 UI 는 떴는데 입력은 게임에 남는, 더 나쁜 상태가 되기 때문이다.
	 *
	 * @param Requester  포커스를 요청하는 객체. 풀 때 같은 객체를 넘겨야 한다
	 * @param Mode       게임 입력을 살릴 것인가
	 */
	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Input")
	void EnterUIFocus(UObject* Requester, EHHUIFocusMode Mode = EHHUIFocusMode::GameAndUI);

	/**
	 * UI 조작 모드에서 나온다.
	 *
	 * **포커스를 쥔 객체만 풀 수 있다.** 남이 부르면 조용히 무시된다 — 이것이 이 한 쌍이
	 * 존재하는 이유다. 아무나 풀 수 있으면 UI 가 겹쳤을 때 먼저 닫힌 쪽이 아직 열려 있는
	 * 쪽의 입력까지 되돌린다.
	 *
	 * 쥐고 있던 객체가 이미 파괴됐다면 누구든 치울 수 있다. 안 그러면 영영 안 풀린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Input")
	void ExitUIFocus(UObject* Requester);

	UFUNCTION(BlueprintPure, Category = "HeavyHanded|Input")
	bool IsUIFocused() const { return bUIFocused; }

	/** 지금 포커스를 쥔 객체. 아무도 없거나 이미 파괴됐으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "HeavyHanded|Input")
	UObject* GetUIFocusOwner() const { return UIFocusOwner.Get(); }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 화면에 붙일 HUD 위젯. BP 에서 값으로 지정한다.
	 *
	 * C++ 기본값을 주지 않는다 — 은신처와 작업 레벨이 서로 다른 위젯을 쓰는데 한쪽 값을
	 * 박아 두면, 지정을 잊은 쪽이 남의 HUD 를 **조용히** 띄운다. 비어 있으면 경고가 뜬다.
	 * AHeistGameMode::SiteTag 에 기본값을 주지 않는 것과 같은 이유다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "HeavyHanded|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

private:
	/** 입력 모드를 실제로 적용한다. 로컬 컨트롤러에서만 의미가 있다 */
	void ApplyInputMode(bool bInUIFocused, EHHUIFocusMode Mode);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HUDWidget;

	/**
	 * 포커스를 쥔 객체. 약참조인 이유는 UI 를 연 액터가 포커스를 풀지 않고 파괴될 수 있기
	 * 때문이다 — 강참조로 붙들면 그 액터가 GC 되지 않고, 포커스도 영영 안 풀린다.
	 */
	TWeakObjectPtr<UObject> UIFocusOwner;

	/**
	 * 지금 UI 포커스 상태인가. 소유자 약참조와 따로 두는 이유는 소유자가 먼저 파괴될 수
	 * 있기 때문이다. 소유자 유효성으로 상태를 판단하면, 소유자가 사라진 순간 코드는
	 * "포커스 없음" 으로 보는데 화면에는 커서가 그대로 남아 둘이 어긋난다.
	 */
	bool bUIFocused = false;
};
