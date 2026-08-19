#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Templates/SubclassOf.h"       // HUDWidgetClass 를 값으로 보유 — 전방 선언 불가
#include "HeavyHUD.generated.h"

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
 * [C++ 과 BP 의 경계] 언제 만들고 언제 붙이고 떼는지는 C++ 이 정한다.
 *   BP 서브클래스가 정하는 것은 HUDWidgetClass 에 어떤 WBP 를 꽂을지 하나뿐이다.
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
	 * 다시 붙였을 때 게이지가 0 부터 시작한다. 결과 화면처럼 잠시 가리는 용도다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void SetHUDVisible(bool bVisible);

	/** HUD 가 지금 보이는 상태인가 */
	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	bool IsHUDVisible() const;

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

private:
	/** HUD 위젯을 만들어 화면에 붙인다. 실패하면 이유를 로그로 남긴다 */
	void CreateAndAddHUDWidget();

	UPROPERTY()
	TObjectPtr<UUserWidget> HUDWidget;
};
