#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"                            // FGameplayTag — 델리게이트 시그니처라 전방 선언 불가
#include "Core/HeistPhase.h"                                 // EHeistPhaseReason — 같은 이유
#include "Core/PlayerControllers/HeavyHandedPlayerController.h"
#include "HeistPlayerController.generated.h"

class APlayerState;
class AGameStateBase;
class AHeistGameState;
class UHeistSpectatorComponent;

/**
 * 작업 레벨의 PlayerController. 페이즈에 따라 화면과 입력을 맞추고, 결과 확인을 서버로 올리고,
 * 관전자의 시점을 팀원에게 붙인다. **판정은 하나도 하지 않는다** —
 * 여기서 조금이라도 판정하면 클라이언트마다 다른 답이 나온다.
 */
UCLASS()
class HEAVYHANDED_API AHeistPlayerController : public AHeavyHandedPlayerController
{
	GENERATED_BODY()

public:
	AHeistPlayerController();

	/**
	 * 결과 화면을 확인했다고 서버에 알린다. HUD 의 '확인' 버튼이 부른다.
	 * GameState 는 소유자가 없어 클라이언트 RPC 를 못 받는다 — 그래서 여기가 유일한 입구다.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Heist|Result")
	void Server_SetResultConfirmed(bool bConfirmed);

	/** 시선 pitch 복제 */
	UFUNCTION(Server, Reliable, WithValidation, Category = "Heist|Spectate")
	void Server_SetSpectateTarget(APlayerState* Target);

	UFUNCTION(BlueprintPure, Category = "Heist|Spectate")
	UHeistSpectatorComponent* GetSpectatorComponent() const { return SpectatorComponent; }

	/** 관전 카메라 체인을 로그로 찍는다 (치트) */
	void DumpSpectateCamera() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	/**
	 * 페이즈가 바뀌었다. 화면과 입력을 여기 한 곳에서 맞춘다.
	 * 서버 · 클라 양쪽에서 불리므로 **표현만** 둔다 — 판정을 두면 호스트에서만 두 번 돈다.
	 */
	UFUNCTION()
	void HandlePhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason);

	/**
	 * 접속 대기 상태가 바뀌었다. 기다리는 동안 게임 입력을 막는다.
	 * 폰은 접속하자마자 스폰되므로, 막지 않으면 먼저 들어온 사람만 저택을 돌아다녀
	 * 준비 시간이 시작될 때 각자 다른 자리에 서 있게 된다.
	 */
	UFUNCTION()
	void HandleStartWaitChanged(FHeistStartWaitState State);

	/**
	 * 로딩 표시를 그린다. BP 는 여기서 위젯만 띄우고 내린다 — 입력 차단은 C++ 가 이미 했다.
	 *
	 * NumExpected 가 0 이면 인원을 모른다는 뜻이다. 그때는 "3/0" 대신 인원 없이 표시할 것.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Heist|Start")
	void OnStartWaitChanged(bool bWaiting, int32 NumConnected, int32 NumExpected);

private:
	/**
	 * GameState 가 도착하면 구독한다. BeginPlay 가 GameState 복제보다 먼저 돌 수 있고,
	 * 구독을 놓치면 **그 클라이언트만** 페이즈 전환에 영영 반응하지 않는다 —
	 * 크래시도 경고도 없이 "쟤 화면만 아직 준비 시간이래요" 로만 드러난다.
	 */
	void BindToGameState(AGameStateBase* GameState);

	void UnbindFromGameState();

	FDelegateHandle GameStateSetHandle;

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> BoundGameState;

	UPROPERTY()
	TObjectPtr<UHeistSpectatorComponent> SpectatorComponent;

	void OnSpectateNext();
	void OnSpectatePrev();
};
