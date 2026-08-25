#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanZone.generated.h"

struct FHeistLoadEntry;
class ABaseCharacter;
class ACharacter;
class ALootBase;
class APawn;
class APlayerState;
class UBoxComponent;
class UDecalComponent;
class UMaterialInterface;
class UNiagaraSystem;

/**
 * 밴. 적재 판정과 승차 처리. 레벨당 하나이고, 판정은 전부 서버다.
 * 몸체는 BP 가 이 액터에 붙인다 — 별도 액터로 두면 PlaceVan 이 이 액터만 옮겨 껍데기만 남는다.
 */
UCLASS()
class HEAVYHANDED_API AVanZone : public AActor
{
	GENERATED_BODY()

public:
	AVanZone();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** 이 레벨의 밴. 레벨당 하나라는 계약 위에서만 성립한다 */
	UFUNCTION(BlueprintPure, Category = "Van", meta = (WorldContext = "WorldContext"))
	static AVanZone* Get(const UObject* WorldContext);

	/**
	 * 탑승 중이면 내려 주고 true. 아니면 아무 일도 하지 않고 false. (서버 전용)
	 * UGAB_Interact 는 시선 스윕보다 **먼저** 부른다 — 탑승하면 밴을 다시 겨눌 수 없다.
	 */
	static bool TryDisembarkIfBoarded(APawn* Player);

	/**
	 * 승차 / 하차를 전환한다. 상호작용이 들어오는 유일한 입구다. (서버 전용)
	 * 탈 수 있는 상황인지는 전부 여기서 본다. 상태가 실제로 바뀌었으면 true.
	 */
	bool TryToggleBoarding(APawn* Player);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 액터의 기준점. **밴 바닥 중앙, 로컬 +X 가 뒷문 방향.** VanAnchor 화살표가 여기로 온다.
	 * 아래 컴포넌트들의 위치 · 크기 · 회전은 뷰포트에서 정한다. 코드는 덮어쓰지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<USceneComponent> VanRoot;

	/** 적재 판정 볼륨. 프로파일 VanLoadZone. 크기와 위치는 뷰포트에서 정한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> LoadVolume;

	/**
	 * 승차 가능 범위. 프로파일 VanBoardZone.
	 * 오버랩 콜백을 걸지 않는다 — 상호작용이 들어온 순간 "지금 이 안에 있는가" 만 답한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> BoardVolume;

	/**
	 * 승차 상호작용의 조준 대상. 뒷문 **개구부**를 덮는 안 보이는 판이다.
	 * 움직이는 문짝에 걸면 문이 열리는 순간 조준 대상이 옆으로 돌아간다.
	 * 콜리전은 Visibility 만 Block — Pawn 을 막으면 열린 뒷문이 벽이 된다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> BoardAimTarget;

	/**
	 * 좌석 앵커. 좌석이 모자라면 마지막 자리에 겹쳐 앉힌다.
	 * 앵커는 '발이 닿는 지점' 이다 — 캡슐 절반 높이는 코드가 더한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van|Seats")
	TArray<TObjectPtr<USceneComponent>> Seats;

	/** 하차 지점. 좌석과 달리 **걸어 다닐 수 있는 바닥**이어야 한다. 발이 닿는 지점으로 해석한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van|Seats")
	TObjectPtr<USceneComponent> ExitAnchor;

	/**
	 * 존의 범위를 바닥에 그리는 데칼. 트랜스폼과 크기는 SyncBorderDecal 이 정한다.
	 * 머티리얼은 Material Domain 이 `Deferred Decal` 이어야 한다 — Surface 면 조용히 안 그려진다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UDecalComponent> BorderDecal;

	/** 테두리 데칼 머티리얼. 비워 두면 데칼이 꺼지고 디버그 박스로 대신 그린다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UMaterialInterface> BorderMaterial;

	/** 적재 확정 이펙트. 모든 머신에서 확정 지점에 재생된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UNiagaraSystem> LoadedEffect;

	/**
	 * 보류 중인 노획물을 다시 검사하는 주기(초).
	 * 체류 시간(UHeistSettings::LoadDwellSeconds)을 재는 해상도라 그보다 충분히 작아야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van",
		meta = (ClampMin = "0.02", Units = "s"))
	float RecheckIntervalSeconds = 0.1f;

	/** 적재 판정의 확정 · 대기 · 거부를 사유와 함께 로그와 화면에 남긴다. (테스트용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Debug")
	bool bShowLoadDebug = false;

	/**
	 * 확정된 노획물을 처리한다. 이 함수를 지나면 적재는 이미 끝나 있다. (서버 전용)
	 * 기본 구현은 이펙트 재생 후 파괴. 남겨 두도록 재정의하면 콜리전을 직접 정리할 것 —
	 * 살려 두면 다음 오버랩에 다시 걸린다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Van")
	void HandleConfirmedLoot(ALootBase* Loot);
	virtual void HandleConfirmedLoot_Implementation(ALootBase* Loot);

	/**
	 * 확정 이펙트를 전원에게 재생한다.
	 * 노획물이 아니라 이 존이 보낸다 — 곧 파괴되는 액터의 채널은 도착이 보장되지 않는다.
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
	/** 지금 이 사람이 탈 수 있는가. 준비 시간에는 못 탄다. 거부 사유를 로그로 남긴다 */
	bool CanBoardNow(const APawn* Player) const;

	/**
	 * 탑승한 폰을 좌석에 앉히거나 내려놓는다. (서버 전용, 결과는 복제된다)
	 * 이동 차단은 임시다 — State.InVan 에 Block.Movement 를 물리는 GE(전영배)가 생기면 걷어낸다.
	 */
	void ApplyBoardedPawnState(APawn* Player, bool bBoarded);

	/**
	 * 빈 좌석을 배정한다. 이미 앉아 있으면 그 자리를 돌려준다. 좌석이 없으면 nullptr.
	 * 비었는지를 점유자의 생존과 승차 명단으로 본다 — 끊긴 사람의 자리가 영영 잠기지 않게.
	 */
	USceneComponent* TakeSeat(APlayerState* Player);

	/** 좌석을 비운다. 앉아 있지 않았으면 아무 일도 하지 않는다 */
	void ReleaseSeat(const APlayerState* Player);

	/** 캐릭터를 앵커 위치에 세운다. 앵커는 발이 닿는 지점이라 캡슐 절반 높이를 더한다 */
	static void PlaceAtAnchor(ACharacter* Character, const USceneComponent* Anchor);

	/** 승차자의 ASC 로 Event.Player.BoardedVan 을 보낸다. 내릴 때는 보내지 않는다 */
	void SendBoardedEvent(APawn* Player, int32 NumBoarded) const;

	/**
	 * 지금 적재를 받는 페이즈인가. 판 전체의 사정만 본다.
	 * 운반자 여부와 체류 시간은 여기서 보지 않는다 — 그건 '거부' 가 아니라 '대기' 다.
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

	/**
	 * 테두리 데칼을 볼륨에 맞춘다. 위치 · 회전 · 스케일은 월드 기준으로 직접 지정한다 —
	 * 부모를 물려받으면 볼륨을 회전시켰을 때 투영 축이 같이 돌아 테두리가 반만 그려진다.
	 */
	void SyncBorderDecal();

	/** 머티리얼이 없을 때 존 범위를 디버그 선으로 그린다. 밴이 움직이므로 타이머로 반복 호출된다 */
	void DrawBorderFallback() const;

	/** 레벨에 이 액터가 둘 이상이면 경고한다 */
	void WarnIfDuplicateZone() const;

	void ShowLoadDebug(const FString& Message, const FColor& Color, const FVector& Location) const;

	/**
	 * 존 안에 있고 아직 확정되지 않은 노획물과 '손을 떠난 시각'(월드 시간).
	 * 값이 음수면 아직 들고 있다는 뜻이다 — 그동안은 체류 시간이 흐르지 않는다.
	 * 키가 약참조인 것은 여기 있는 동안 파손형이 파괴될 수 있어서다.
	 */
	TMap<TWeakObjectPtr<ALootBase>, float> PendingLoot;

	/**
	 * 좌석별 점유자. 인덱스는 Seats 와 같다.
	 * 복제하지 않는다 — 클라이언트에 필요한 "누가 어디에 붙어 있는가" 는 어태치 복제가 전달한다.
	 */
	TArray<TWeakObjectPtr<APlayerState>> SeatOccupants;

	FTimerHandle RecheckTimerHandle;

	/** 테두리 폴백 재그리기. BorderMaterial 이 비어 있을 때만 돈다 */
	FTimerHandle BorderDebugTimerHandle;
};
