#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlipTrap.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraSystem;
class USoundBase;
class ABaseCharacter;

/**
 * 밟으면 미끄러져 넘어지는 함정(바나나 껍질). 임의 방향으로 살짝 튕겨 나가고,
 * 다운 상태가 되며, 뭔가 들고 있었다면 놓친다.
 *
 * [AMovementTrap 을 더 이상 재사용하지 않는 이유]
 *   처음에는 "밟으면 잠깐 못 움직인다" 는 점이 같아서 AMovementTrap 에 값만
 *   다르게 얹었었다. 그런데 실제로 원하는 동작(넉백 + 다운 + 아이템 드롭)은
 *   그 클래스의 CharacterMovementComponent::DisableMovement() 로는 표현이 안 되고,
 *   전부 State.Downed 시스템이 이미 하는 일과 겹친다 — 새로 만드는 대신 그 시스템에
 *   이벤트만 보내는 쪽이 맞다.
 *
 * [다운은 여기서 직접 만들지 않는다 — AVanZone 과 같은 방식으로 이벤트만 보낸다]
 *   State.Downed 로 전환되는 실제 판정(태그 부여, 복구 로직 등)은 전부 캐릭터 파트
 *   (GAB_Downed)가 갖고 있다. 여기서는 AVanZone::SendLoadedEvent 와 똑같이
 *   UAbilitySystemBlueprintLibrary::SendGameplayEventToActor 로
 *   Event.Player.Downed 하나만 보낸다 — 캐릭터 파일을 한 줄도 안 건드린다.
 *
 * [순서 — 넉백 먼저, 다운 이벤트는 나중]
 *   다운 상태가 되고 나면 이동이 막힐 수 있어서, 그 뒤에 LaunchCharacter 를 걸면
 *   넉백이 씹힐 위험이 있다. 그래서 아직 정상 이동 중인 지금 먼저 띄우고,
 *   아이템을 놓게 한 다음, 마지막에 다운 이벤트를 보낸다.
 *
 * [경비는 안 걸린다]
 *   OtherActor 를 ABaseCharacter 로만 캐스트한다. AGuardCharacter 는 다른 클래스라
 *   자기 순찰 중에 자기 함정에 걸리는 일이 없다 (다른 Hazard 클래스들과 동일 원칙).
 *
 * [DestroyAfterTrigger — 바나나 껍질은 한 번 쓰면 없어진다]
 *   기본 true. 연출이 끝난 뒤(DestroyDelay) 사라진다 — AVaultDoor::DoorHideDelay 와
 *   같은 이유로, 밟는 순간과 사라지는 순간을 프레임 하나에 몰지 않는다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API ASlipTrap : public AActor
{
	GENERATED_BODY()

public:
	ASlipTrap();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 시각 전용. 콜리전 없음 — 판정은 TriggerVolume 이 한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> PeelMesh;

	/** 밟았는지 판정하는 트리거 볼륨 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> TriggerVolume;

	// 단위: cm/s (UE 에는 cm/s 단위 지정자가 없어 Units 메타 없이 클램프만 건다 — GuardTypes.h 와 동일 사유)
	/** 미끄러지며 튕겨 나가는 수평 속도. 방향은 매번 무작위다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Slip",
		meta = (ClampMin = "0.0"))
	float SlipLaunchStrength = 500.f;

	/** 수평 속도 대비 위로 뜨는 비율. 0 이면 바닥에 붙어서 미끄러지듯, 크면 붕 뜬다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Slip",
		meta = (ClampMin = "0.0"))
	float SlipUpwardRatio = 0.3f;

	/** 한 번 걸린 뒤 사라지는가. 바나나 껍질은 소모성이라 기본 true */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Slip")
	bool bDestroyAfterTrigger = true;

	/** bDestroyAfterTrigger 가 true 일 때, 연출이 나온 뒤 이만큼 지나서 사라진다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Slip",
		meta = (ClampMin = "0.0", Units = "s", EditCondition = "bDestroyAfterTrigger"))
	float DestroyDelay = 0.5f;

	// ---- 연출 (BP 는 에셋만 고른다) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<UNiagaraSystem> SlipEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Visual")
	TObjectPtr<USoundBase> SlipSound;

private:
	/** 미끄러지는 순간의 연출만 전달한다. 상태를 남기지 않으므로 Unreliable 이다 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlaySlipEffect();
	void Multicast_PlaySlipEffect_Implementation();

	/**
	 * DestroyDelay 타이머가 부르는 래퍼. AActor::Destroy() 는 반환값·매개변수가 있어
	 * FTimerManager::SetTimer 가 요구하는 void() 시그니처에 그대로 못 물린다.
	 */
	void DestroySelf();

	FTimerHandle DestroyTimerHandle;
};
