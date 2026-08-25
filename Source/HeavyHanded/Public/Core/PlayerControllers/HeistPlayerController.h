#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"                            // FGameplayTag — 델리게이트 시그니처라 전방 선언 불가
#include "Core/HeistPhase.h"                                 // EHeistPhaseReason — 같은 이유
#include "Core/PlayerControllers/HeavyHandedPlayerController.h"
#include "HeistPlayerController.generated.h"

class AGameStateBase;
class AHeistGameState;

/**
 * 작업 레벨(저택 · 박물관 · 은행)의 PlayerController.
 *
 * [무엇을 하는가] 페이즈에 따라 화면과 입력을 맞추고, 결과 화면의 '확인' 을 서버로 올리고,
 *   관전자의 시점을 팀원에게 붙인다.
 *   **판정은 하나도 하지 않는다** — 등급도 체포도 종료도 AHeistGameMode 와 AHeistGameState 의
 *   몫이다. 여기서 조금이라도 판정하면 클라이언트마다 다른 답이 나온다.
 *
 * [은신처와 나눈 이유] 이 클래스는 페이즈 상태머신에 통째로 매여 있는데 은신처에는 페이즈가
 *   없다. 한 클래스에 넣으면 OnPhaseChanged 구독과 상점 RPC 가 한 파일에 섞이고, 코어 루프와
 *   세션 파트가 같은 파일을 고치게 된다. ServerTravel 로 어차피 인스턴스가 갈리므로
 *   합쳐도 상태를 공유하지는 못한다 — 합칠 이득이 없다.
 */
UCLASS()
class HEAVYHANDED_API AHeistPlayerController : public AHeavyHandedPlayerController
{
	GENERATED_BODY()

public:
	/**
	 * 결과 화면을 확인했다고 서버에 알린다. HUD 의 '확인' 버튼이 부른다.
	 *
	 * [왜 컨트롤러를 거치는가] AHeistGameState 는 소유자(Owner)가 없어 클라이언트가 보내는
	 *   RPC 를 받을 수 없다. 결과 확인은 반드시 PlayerController 를 통과해야 하고, 여기가
	 *   그 입구다. GameState 헤더 SetResultConfirmed 주석이 가리키는 자리이기도 하다.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Heist|Result")
	void Server_SetResultConfirmed(bool bConfirmed);

	// ──────────────────────────────────────────────
	// 관전 (체포된 사람의 시점)
	//
	// [왜 전부 로컬인가] 시점은 순수 표현이다. APlayerController::SetViewTarget 은 로컬
	//   PlayerCameraManager 만 만지고 복제되지 않으므로(엔진이 따로 ClientSetViewTarget 을
	//   두고 있는 이유), 서버가 골라 밀어 줄 게 아니라 각자 자기 화면에 대해 고르면 된다.
	//   판정도 아니라서 서버 권위가 필요하지 않다.
	//
	// [자유 비행이 아닌 이유] AHeistGameMode 가 SpectatorClass 를 비워 관전 폰을 없앴다.
	//   벽을 통과해 날아다니면 체포자가 저택을 훑어 경비 · 트랩 · 금고 위치를 음성으로
	//   알려 줄 수 있고, 그러면 잠입 게임이 무너진다. 카메라는 팀원 시점에만 붙는다.
	// ──────────────────────────────────────────────

	/** 관전 시점을 다음 팀원으로 넘긴다. HUD 버튼이나 입력 바인딩이 부른다 */
	UFUNCTION(BlueprintCallable, Category = "Heist|Spectate")
	void ViewNextTeammate();

	/** 관전 시점을 이전 팀원으로 넘긴다 */
	UFUNCTION(BlueprintCallable, Category = "Heist|Spectate")
	void ViewPreviousTeammate();

	/** 지금 이 화면이 관전 중인가. 관전자가 아니면 false */
	UFUNCTION(BlueprintPure, Category = "Heist|Spectate")
	bool IsSpectating() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 페이즈가 바뀌었다. 화면과 입력을 여기 한 곳에서 맞춘다.
	 *
	 * 서버 · 클라이언트 양쪽에서 불린다 (서버는 직접 방송, 클라이언트는 RepNotify).
	 * 그래서 여기에는 **표현만** 둔다 — 판정을 두면 호스트에서만 두 번 도는 코드가 된다.
	 *
	 * 페이즈 진입 부수효과가 늘어나면 EnterUIFocus 옆에 조건문을 쌓지 말고, GameMode 의
	 * OnPhaseEntered 처럼 표현 쪽도 한 곳에 모을 것.
	 */
	UFUNCTION()
	void HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason);

private:
	/**
	 * GameState 가 도착하면 구독한다.
	 *
	 * [왜 BeginPlay 에서 바로 못 하는가] 클라이언트에서는 PlayerController 의 BeginPlay 가
	 *   GameState 복제보다 먼저 돌 수 있다. 그때 GetGameState() 는 nullptr 이고, 구독을 놓치면
	 *   **그 클라이언트만** 페이즈 전환에 영영 반응하지 않는다 — 크래시도 경고도 없고,
	 *   "쟤 화면만 아직 준비 시간이래요" 로만 드러난다.
	 *   UNoiseSubsystem 이 UAlertComponent 를 붙일 때 쓰는 것과 같은 패턴이다.
	 */
	void BindToGameState(AGameStateBase* GameState);

	void UnbindFromGameState();

	/**
	 * 관전 상태를 주기적으로 확인한다.
	 *
	 * [왜 폴링인가] 기다리는 것이 둘인데 알림이 오는 것은 하나도 없다 — 내 PlayerState 의
	 *   도착(그래야 관전자인지 알 수 있다)과 팀원 폰의 복제(그래야 볼 대상이 생긴다).
	 *   레벨이 열린 직후엔 둘 다 없다. 관전자가 아니라고 판명되면 타이머를 끄므로
	 *   실제 플레이어에게는 몇 번의 포인터 검사로 끝난다.
	 */
	void TickSpectate();

	/** 볼 대상이 없거나 사라졌으면 다시 고른다 */
	void EnsureSpectateTarget();

	/**
	 * 관전 대상을 고른다.
	 *
	 * @param Step  0 이면 현재 대상을 유지(없으면 첫 번째), +1/-1 이면 순환
	 * @return      볼 폰. 볼 사람이 아무도 없으면 nullptr
	 */
	AActor* PickSpectateTarget(int32 Step) const;

	FDelegateHandle GameStateSetHandle;

	FTimerHandle SpectateTimer;

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> BoundGameState;
};
