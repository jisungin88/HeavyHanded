#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanLoadZone.generated.h"

class ALootBase;
class APawn;
class UBoxComponent;
class USceneComponent;

/**
 * 밴 화물칸의 적재 판정 볼륨. 코어 루프에서 "돈이 실제로 들어오는" 유일한 지점이다.
 *
 * [무엇을 판정하고 무엇을 판정하지 않는가]
 *   여기가 보는 것    존 안에 들어왔는가 / 지금 누가 들고 있는가
 *   보지 않는 것      가치가 얼마인가 · 파손됐는가 · 무거운가 — 전부 노획물 파트(김민준) 소관이다
 *                     ALootBase::GetCurrentValue() 하나만 물어보고 그 값을 그대로 믿는다.
 *
 *   (기술설계서 코어 루프 5장 "경계 — 이게 적재 가능한 물건인가는 아이템 파트가 판정한다")
 *
 * [확정 조건 — 존 안 + 운반자 없음]
 *   들고 서 있는 것만으로 적재되면 안 된다. 밴 옆을 지나가기만 해도 돈이 들어오고,
 *   "실을까 말까" 라는 판단 자체가 사라진다. 손에서 떠나야 적재다.
 *
 *   그래서 오버랩 시점에 들려 있으면 확정을 보류하고 주기적으로 다시 검사한다.
 *   놓기는 오버랩 이벤트를 새로 만들지 않기 때문이다 — 물건은 이미 존 안에 있고
 *   바뀌는 것은 PrimaryCarrier 뿐이라, 다시 물어보지 않으면 영원히 확정되지 않는다.
 *
 *   던져 넣기는 처음부터 운반자가 없으므로 오버랩 즉시 확정된다. 기획상 정식 적재 경로다.
 *
 * [한 번 실으면 못 꺼낸다] 확정 시 콜리전을 끄고 밴에 어태치하므로 다시 집을 수 없다.
 *   설계 문서의 TODO("적재 후 다시 꺼낼 수 있는가")에 대한 답을 '아니오' 로 정한 것이다.
 *   꺼내기를 허용하면 AHeistGameState 의 누적액이 감소해야 하고, 그러면 목표 달성 판정이
 *   달성 → 미달성으로 되돌아간다. 되돌아가는 승리 조건은 페이즈 전이(단방향)와 맞지 않는다.
 *
 * 판정은 전부 서버다. 클라이언트는 LoadedLoot 복제로 같은 모습만 만든다.
 */
UCLASS()
class HEAVYHANDED_API AVanLoadZone : public AActor
{
	GENERATED_BODY()

public:
	AVanLoadZone();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 지금까지 이 존이 확정한 노획물.
	 *
	 * 결과 화면의 '적재 목록'(기획서 8장)이 여기서 나온다. 금액 자체는 AHeistGameState 가 들고 있고
	 * 여기 있는 것은 무엇이 실렸는지의 목록이다 — 둘을 합치지 않는 이유는 GameState 쪽이
	 * 매치당 하나여야 하는 진리원이고, 이 액터는 레벨에 여러 개 놓일 수 있기 때문이다.
	 */
	const TArray<TObjectPtr<ALootBase>>& GetLoadedLoot() const { return LoadedLoot; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 적재 판정 볼륨이자 루트. 프로파일은 VanLoadZone (Config/DefaultEngine.ini) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> LoadVolume;

	/**
	 * 확정된 노획물을 붙일 대상. 비워 두면 이 액터의 루트(= 볼륨)에 붙는다.
	 *
	 * 이 존을 밴 액터에 어태치해 배치했다면 비워 두는 것이 맞다 — 볼륨이 이미 밴을 따라간다.
	 * 밴과 따로 놓아야 하는 배치(고정 하역장 등)에서만 지정한다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Van")
	TObjectPtr<AActor> VanActor;

	/**
	 * 보류 중인 노획물을 다시 검사하는 주기(초).
	 *
	 * 놓는 순간과 적재 확정 사이의 지연이 곧 이 값이다. 0.25초면 사람 눈에는 즉시로 보이고,
	 * 화물칸에 물건이 여러 개 쌓여 있어도 검사 대상은 '지금 들려 있는 것' 뿐이라 비용이 없다.
	 * 검사할 것이 없으면 타이머 자체가 멈춘다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van",
		meta = (ClampMin = "0.05", Units = "s"))
	float RecheckIntervalSeconds = 0.25f;

	/**
	 * 적재 판정 과정을 로그와 화면에 남긴다. (테스트용, 기본 꺼짐)
	 *
	 * 확정뿐 아니라 보류·거부도 사유와 함께 찍는다. "밴에 넣었는데 돈이 안 오른다" 는
	 * 신고가 오면 존 밖이었는지 · 아직 들고 있었는지 · 이미 실은 것이었는지를 갈라야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Debug")
	bool bShowLoadDebug = false;

	/**
	 * 확정된 노획물 목록. 클라이언트는 이걸 받아 같은 모습을 만든다.
	 *
	 * [왜 복제하는가] 물리 OFF + 어태치는 복제되는 상태가 아니다. 서버에서만 끄면
	 * 클라이언트의 노획물은 계속 물리를 돌려 밴 바닥을 뚫고 내려가거나 뒤로 흘러내린다.
	 * ALootBase 가 소지 상태를 OnRep_PrimaryCarrier 로 맞추는 것과 같은 이유다.
	 *
	 * [왜 멀티캐스트 RPC 가 아닌가] RPC 는 늦게 들어온 클라이언트에게 가지 않는다.
	 * 재접속한 사람 화면에서만 화물이 쏟아지는 버그가 되고, 그런 것은 재현이 어렵다.
	 * 목록은 적재 순간에만 갱신되므로 대역폭은 사실상 문제되지 않는다.
	 *
	 * [순서 의존] 이 복제가 ALootBase::PrimaryCarrier 의 '비워짐' 보다 먼저 도착하면,
	 * 뒤늦게 도는 OnRep_PrimaryCarrier → ApplyCarryState 가 물리를 도로 켜 버린다.
	 * 두 액터의 복제 순서는 보장되지 않지만 놓기와 확정 사이에 최소 RecheckIntervalSeconds,
	 * 던지기는 체공 시간이 끼어 있어 실제로 같은 프레임에 겹치지 않는다.
	 * 재검사 주기를 0 에 가깝게 줄이면 이 가정이 깨진다 — ClampMin 을 그래서 둔다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_LoadedLoot, VisibleInstanceOnly, Category = "Van")
	TArray<TObjectPtr<ALootBase>> LoadedLoot;

	UFUNCTION()
	void OnRep_LoadedLoot();

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	/**
	 * 지금 적재를 받는 페이즈인가. 노획물의 상태와는 무관한, 판 전체의 사정이다.
	 *
	 * 운반자 여부를 여기서 보지 않는 이유는 그것이 '거부' 가 아니라 '보류' 이기 때문이다.
	 * 둘을 한 함수에 넣으면 false 하나로 "영영 안 됨" 과 "이따 다시" 가 구별되지 않는다.
	 */
	bool IsLoadAllowedNow() const;

	/** 아직 들려 있는 노획물을 보류 목록에 넣고 재검사 타이머를 켠다. (서버 전용) */
	void HoldForRecheck(ALootBase* Loot);

	/** 적재를 확정한다. 여기서만 금액이 오른다. (서버 전용) */
	void ConfirmLoad(ALootBase* Loot);

	/** 물리 OFF · 콜리전 OFF · 밴에 어태치. 서버와 클라이언트 양쪽에서 같은 일을 한다 */
	void ApplyLoadedState(ALootBase* Loot) const;

	/** 적재자의 ASC 로 Event.Loot.Loaded 를 보낸다. 적재자를 모르면 아무것도 하지 않는다 */
	void SendLoadedEvent(APawn* Loader, ALootBase* Loot, int32 LoadedValue) const;

	/** 보류 중인 것들을 다시 검사한다. 운반자가 사라졌으면 그때 확정한다 */
	void RecheckPending();

	/** 보류 목록이 비어 있지 않을 때만 타이머가 돈다 */
	void UpdateRecheckTimer();

	/** 노획물을 붙일 대상. VanActor 가 없으면 이 액터의 루트 */
	USceneComponent* GetAttachTarget() const;

	void ShowLoadDebug(const FString& Message, const FColor& Color, const FVector& Location) const;

	/**
	 * 존 안에 들어왔지만 아직 누군가 들고 있는 노획물.
	 *
	 * 약참조인 이유는 여기 있는 동안 노획물이 파괴될 수 있기 때문이다 (파손형).
	 * 그때는 다음 검사에서 조용히 빠진다.
	 */
	TSet<TWeakObjectPtr<ALootBase>> PendingLoot;

	FTimerHandle RecheckTimerHandle;
};
