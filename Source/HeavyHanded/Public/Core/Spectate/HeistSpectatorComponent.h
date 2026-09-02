#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Spectate/HeistSpectateTypes.h"
#include "HeistSpectatorComponent.generated.h"

class APlayerController;
class APlayerState;

UCLASS( ClassGroup=(Heist), meta=(BlueprintSpawnableComponent) )
class HEAVYHANDED_API UHeistSpectatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeistSpectatorComponent();

	/** 지금 관전 중인 플레이어. 없으면 nullptr */
	UFUNCTION(BlueprintPure, Category = "Heist|Spectate")
	APlayerState* GetViewedPlayer() const;

	/** 이 화면이 관전 중인지 체크 */
	UFUNCTION(BlueprintPure, Category = "Heist|Spectate")
	bool IsSpectating() const;

	/** 관전자에게 보여줄 정보의 범위 */
	UFUNCTION(BlueprintPure, Category = "Heist|Spectate")
	EHeistSpectateInfoLevel GetInfoLevel() const;

	/** 관전 시점을 다음으로 */
	UFUNCTION(BlueprintCallable, Category = "Heist|Spectate")
	void ViewNext();

	/** 관전 시점을 이전으로 */
	UFUNCTION(BlueprintCallable, Category = "Heist|Spectate")
	void ViewPrevious();

	// -------------------------
	// 치트
	//-------------------------

	/** 관전 상태와 후보 목록 로그 */
	void DumpSpectateState() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 소유 PlayerController */
	APlayerController* GetOwningPC() const;

	/** 볼 수 있는 팀원 */
	void BuildCandidates(TArray<APlayerState*>& Out) const;

	/** 커서에서 Step만큼 옮긴다. */
	APlayerState* PickTarget(int32 Step) const;

	/** 커서 갱신 */
	void ApplyViewTarget(APlayerState* Target);

	/** 보고 있는 사람 */
	TWeakObjectPtr<APlayerState> ViewedPlayer;

	/** 관전 상태를 주기적으로 확인한다 */
	void TickSpectate();
	/** 볼 대상이 없거나 사라졌으면 다시 고른다 */
	void EnsureTarget();
	void ViewStep(int32 Step);

	FTimerHandle SpectateTimer;

	/** 관전 입력 매핑이 붙었는지 */
	bool bSpectateInputApplied = false;

	/** 관전 상태에 맞춰 IMC_Spectate 추가/제거 */
	void ApplySpectateInput(bool bEnable);
};
