#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanLoadZone.generated.h"

struct FHeistLoadEntry;
class ALootBase;
class APawn;
class UBoxComponent;
class UDecalComponent;
class UMaterialInterface;
class UNiagaraSystem;

/**
 * 밴 화물칸의 적재 판정 볼륨. 코어 루프에서 "돈이 실제로 들어오는" 유일한 지점이다.
 *
 * [레벨당 하나] 진입 지역에 따라 놓이는 자리는 달라지지만, 한 레벨에 이 액터는 하나뿐이다.
 *   BeginPlay 에서 개수를 세어 둘 이상이면 경고를 남긴다 — 계약이 말로만 남으면
 *   누가 복사해 붙여도 아무도 모르고, 적재액이 어느 존에서 왔는지 추적할 수 없게 된다.
 *
 * [무엇을 판정하고 무엇을 판정하지 않는가]
 *   여기가 보는 것    존 안에 있는가 / 지금 누가 들고 있는가 / 얼마나 오래 있었는가
 *   보지 않는 것      가치가 얼마인가 · 파손됐는가 · 무거운가 — 전부 노획물 파트(김민준) 소관이다
 *                     ALootBase 에 물어본 값을 그대로 믿는다.
 *
 *   (기술설계서 코어 루프 5장 "경계 — 이게 적재 가능한 물건인가는 아이템 파트가 판정한다")
 *
 * [확정 조건 — 운반자 없이 존 안에서 UHeistSettings::LoadDwellSeconds 만큼 머물기]
 *   노획물의 물리는 확정 전까지 그대로 살아 있다. 그래서 오버랩만으로는 판정이 안 된다 —
 *   던져 넣은 물건이 화물칸 바닥에서 튕겨 다시 굴러 나올 수 있고, 그것까지 적재로 세면
 *   밴 쪽으로 던지기만 해도 돈이 들어온다. 체류 시간이 "제대로 들어갔는가" 를 가른다.
 *
 *   들고 있는 동안은 카운트가 멈춘다. 손에서 떠난 순간부터가 시작이다 — 들고 서 있는 것만으로
 *   적재되면 밴 옆을 지나가기만 해도 돈이 들어오고, "실을까 말까" 라는 판단 자체가 사라진다.
 *
 *   놓기와 던지기는 오버랩 이벤트를 새로 만들지 않는다. 물건은 이미 존 안에 있고 바뀌는 것은
 *   운반자뿐이라, 주기적으로 다시 물어보지 않으면 영원히 확정되지 않는다. 재검사 타이머가 그것이다.
 *
 * [확정되면 노획물은 사라진다]
 *   물리를 끄고 화물칸에 붙여 두지 않는다. 실린 물건이 남아 있으면 밴이 움직일 때 흘러내리고,
 *   클라이언트에서 물리가 따로 돌아 바닥을 뚫고, 그걸 맞추려고 어태치 상태를 복제해야 한다.
 *   사라지면 그 전부가 필요 없어진다 — 액터 파괴는 엔진이 알아서 복제한다.
 *
 *   무엇을 실었는지는 확정 순간에 FHeistLoadEntry 로 복사해 AHeistGameState 에 넘긴다.
 *   결과 화면은 액터가 아니라 그 기록을 본다.
 *
 * [연출을 갈아 끼우는 자리] HandleConfirmedLoot() 하나다. 기본 구현은 이펙트 후 파괴이고,
 *   "NPC 가 나와서 밴에 싣는다" 로 바꾸려면 그 함수만 재정의하면 된다.
 *   판정 · 금액 · 이벤트는 그 위에서 이미 끝나 있어 건드릴 것이 없다.
 *
 * 판정은 전부 서버다. 클라이언트가 하는 일은 확정 이펙트를 재생하는 것뿐이다.
 */
UCLASS()
class HEAVYHANDED_API AVanLoadZone : public AActor
{
	GENERATED_BODY()

public:
	AVanLoadZone();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 적재 판정 볼륨이자 루트. 프로파일은 VanLoadZone (Config/DefaultEngine.ini) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> LoadVolume;

	/**
	 * 존의 범위를 바닥에 그리는 데칼. 크기는 볼륨에서 자동으로 따라간다.
	 *
	 * 판정 볼륨은 게임 화면에 보이지 않으므로, 표시가 없으면 플레이어가 "어디까지가 밴인지"
	 * 를 알 방법이 없다. 반투명 박스 대신 바닥 데칼인 이유는 화물칸 안쪽 시야를 가리지 않기
	 * 때문이다 — 물건을 던져 넣는 동안 안이 보여야 한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UDecalComponent> BorderDecal;

	/**
	 * 테두리 데칼에 쓸 머티리얼. 비워 두면 데칼이 꺼지고 디버그 박스로 대신 그린다.
	 *
	 * 폴백을 둔 이유는 그레이박스 때문이다. 머티리얼이 나오기 전에도 레벨 담당이 밴을 배치하고
	 * 존 범위를 눈으로 확인할 수 있어야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UMaterialInterface> BorderMaterial;

	/**
	 * 적재 확정 이펙트. 모든 머신에서 확정 지점에 재생된다.
	 *
	 * 비워 두면 노획물이 아무 연출 없이 사라진다 — 동작에는 지장이 없지만
	 * "실린 것" 과 "어디론가 사라진 것" 이 화면상 구별되지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UNiagaraSystem> LoadedEffect;

	/**
	 * 보류 중인 노획물을 다시 검사하는 주기(초).
	 *
	 * 체류 시간(UHeistSettings::LoadDwellSeconds)을 재는 해상도다. 확정이 최대 이만큼 늦어질 수
	 * 있으므로 체류 시간보다 충분히 작아야 한다. 검사 대상은 존 안에 있는 것뿐이고 하는 일이
	 * 포인터 확인과 뺄셈이라 비용이 없다. 검사할 것이 없으면 타이머 자체가 멈춘다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van",
		meta = (ClampMin = "0.02", Units = "s"))
	float RecheckIntervalSeconds = 0.1f;

	/**
	 * 적재 판정 과정을 로그와 화면에 남긴다. (테스트용, 기본 꺼짐)
	 *
	 * 확정뿐 아니라 대기·거부도 사유와 함께 찍는다. "밴에 넣었는데 돈이 안 오른다" 는
	 * 신고가 오면 존 밖이었는지 · 아직 들고 있었는지 · 체류 시간을 못 채우고 굴러 나갔는지를
	 * 갈라야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Debug")
	bool bShowLoadDebug = false;

	/**
	 * 확정된 노획물을 처리한다. 이 함수를 지나면 적재는 이미 끝나 있다. (서버 전용)
	 *
	 * 기본 구현은 확정 이펙트를 전원에게 재생하고 노획물을 파괴한다.
	 * "NPC 가 나와서 밴에 싣는다" 로 바꾸려면 여기를 재정의해 파괴를 미루고 NPC 에 넘기면 된다 —
	 * 금액 · 기록 · 이벤트는 이미 반영된 뒤라 이 함수가 무엇을 하든 판정은 흔들리지 않는다.
	 *
	 * [재정의할 때의 계약] 노획물을 남겨 두기로 했다면 콜리전을 직접 정리할 것.
	 *   살려 두면 존 안에 그대로 있어 다음 오버랩에 다시 걸린다. 기본 구현은 파괴하므로
	 *   그 문제가 없다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Van")
	void HandleConfirmedLoot(ALootBase* Loot);
	virtual void HandleConfirmedLoot_Implementation(ALootBase* Loot);

	/**
	 * 확정 이펙트를 전원에게 재생한다.
	 *
	 * 노획물 액터가 아니라 이 존이 보낸다. 노획물은 곧 파괴되는데, 파괴되는 액터의 채널로
	 * 보낸 RPC 는 도착 여부를 장담할 수 없다. 존은 레벨 내내 살아 있다.
	 *
	 * Unreliable 인 이유는 연출이기 때문이다. 한 번 놓쳐도 금액과 기록은 이미 복제된다.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayLoadedEffect(FVector Location);

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
	 * 운반자 여부와 체류 시간을 여기서 보지 않는 이유는 그것이 '거부' 가 아니라 '대기' 이기
	 * 때문이다. 셋을 한 함수에 넣으면 false 하나로 "영영 안 됨" 과 "이따 다시" 가 구별되지 않는다.
	 */
	bool IsLoadAllowedNow() const;

	/** 존에 들어온 노획물을 추적 목록에 넣고 재검사 타이머를 켠다. (서버 전용) */
	void TrackPending(ALootBase* Loot);

	/** 추적 중인 것들의 체류 시간을 갱신하고, 다 채운 것을 확정한다. (서버 전용) */
	void RecheckPending();

	/** 추적 목록이 비어 있지 않을 때만 타이머가 돈다 */
	void UpdateRecheckTimer();

	/** 적재를 확정한다. 여기서만 금액이 오른다. (서버 전용) */
	void ConfirmLoad(ALootBase* Loot);

	/** 확정 순간의 사실을 값으로 복사한다. 액터가 사라져도 남아야 하는 것들이다 */
	FHeistLoadEntry MakeLoadEntry(const ALootBase* Loot) const;

	/** 적재자의 ASC 로 Event.Loot.Loaded 를 보낸다. 적재자를 모르면 아무것도 하지 않는다 */
	void SendLoadedEvent(APawn* Loader, ALootBase* Loot, int32 LoadedValue) const;

	/** 데칼 크기를 볼륨에 맞춘다. 에디터에서 볼륨을 늘리면 테두리도 같이 늘어난다 */
	void SyncBorderDecal();

	/** 머티리얼이 없을 때 존 범위를 디버그 선으로 대신 그린다 */
	void DrawBorderFallback() const;

	/** 레벨에 이 액터가 둘 이상이면 경고한다 */
	void WarnIfDuplicateZone() const;

	void ShowLoadDebug(const FString& Message, const FColor& Color, const FVector& Location) const;

	/**
	 * 존 안에 있고 아직 확정되지 않은 노획물과, 그 물건이 '손을 떠난 시각'(월드 시간).
	 *
	 * 값이 음수면 아직 누가 들고 있다는 뜻이다 — 그 상태에서는 체류 시간이 흐르지 않는다.
	 * 들었다 놓기를 반복하면 그때마다 처음부터 다시 센다.
	 *
	 * 키가 약참조인 이유는 여기 있는 동안 노획물이 파괴될 수 있기 때문이다 (파손형).
	 * 그때는 다음 검사에서 조용히 빠진다.
	 */
	TMap<TWeakObjectPtr<ALootBase>, float> PendingLoot;

	FTimerHandle RecheckTimerHandle;
};
