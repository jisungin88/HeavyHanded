#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"        // HUDWidgetClass 를 값으로 보유 — 전방 선언 불가
#include "HeavyHandedPlayerController.generated.h"

class UUserWidget;

/**
 * UI 조작 중 게임 입력을 어떻게 할 것인가. 은신처 단말기는 조작 중에도 주변이 살아 있어야 하고,
 * 결과 화면은 판이 끝났으므로 게임 입력이 들어가면 안 된다.
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
 * 두 레벨이 **실제로 둘 다 쓰는 것**만 둔다 — HUD 를 붙이는 절차와 UI 조작 중의 입력 모드.
 * 위젯이 아니라 절차만 공유한다. Abstract 인 것은 이걸 직접 지정하면 언제나 실수라서다.
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
	 * 로컬 컨트롤러가 아니거나 클래스가 비어 있으면 nullptr. 언제 부를지는 파생 클래스가 정한다.
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
	 * UI 조작 모드로 들어간다. Requester 는 풀 때 같은 객체를 넘겨야 한다.
	 * 소유자를 기록하는 것은 SetInputMode 를 각자 부르면 한 곳이 반드시 복구를 빠뜨리는데,
	 * 그때 화면에 UI 가 안 떠 있어서 **누가 안 놓았는지 알 수 없기** 때문이다.
	 * 이미 남이 쥐고 있으면 경고를 남기고 넘겨받는다 — 거절하면 더 나쁜 상태가 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "HeavyHanded|Input")
	void EnterUIFocus(UObject* Requester, EHHUIFocusMode Mode = EHHUIFocusMode::GameAndUI);

	/**
	 * UI 조작 모드에서 나온다. **포커스를 쥔 객체만 풀 수 있다** —
	 * 아무나 풀 수 있으면 UI 가 겹쳤을 때 먼저 닫힌 쪽이 남의 입력까지 되돌린다.
	 * 쥐고 있던 객체가 이미 파괴됐다면 누구든 치울 수 있다.
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
	 * C++ 기본값을 주지 않는다 — 한쪽 값을 박아 두면 지정을 잊은 쪽이 남의 HUD 를 조용히 띄운다.
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
