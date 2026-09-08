#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Puddle.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UGameplayEffect;
class ABaseCharacter;

/**
 * 안에 있는 동안 이동속도를 늦추는 웅덩이. 나가면 즉시 원래 속도로 돌아온다.
 *
 * [AMovementTrap 과 다르게 GAS 를 거친다 — 이유가 있다]
 *   ABaseCharacter::OnMovementSpeedChanged(BaseCharacter.cpp) 를 보면, 이동속도는
 *   CharacterMovementComponent::MaxWalkSpeed 를 직접 정하는 게 아니라 GAS 의
 *   MovementSpeed 속성이 바뀔 때마다 그 값이 MaxWalkSpeed 에 그대로 덮어써진다.
 *
 *   그래서 여기서 MaxWalkSpeed 를 직접 깎으면, 플레이어가 Sprint 를 시작/종료하거나
 *   다른 효과가 그 속성을 다시 건드리는 순간 우리가 깎은 값이 조용히 원래대로
 *   덮어써진다 — 웅덩이 안에 있는데 갑자기 정상 속도로 보이는 버그가 된다.
 *   AMovementTrap 의 DisableMovement() 는 이 속성과 무관해서 문제가 없었지만,
 *   "속도를 비율로 깎는" 이 효과는 반드시 같은 경로(GAS)를 타야 한다.
 *
 * [그래도 캐릭터 파일은 안 건드린다]
 *   ABaseCharacter::ApplyGameplayEffectToSelf() / RemoveGameplayEffectFromSelf() 가
 *   이미 public 이다(UGA_HeavyCarryAssist 의 과적 페널티와 같은 함수). 이 액터는
 *   그 두 함수만 호출한다 — BaseCharacter.h/.cpp 에 새 코드가 들어가지 않는다.
 *
 * [SlowEffectClass 는 이 폴더 소유다]
 *   GE_ 에셋을 새로 만들어야 하는데, Content/HeavyHanded/Hazards/ 안에 두면
 *   전영배의 어빌리티 콘텐츠 폴더를 안 건드리고 끝난다. 태그도 새로 달지 않는다 —
 *   속성만 깎으면 되므로 "웅덩이에 있다" 는 상태를 다른 시스템이 알아야 할 이유가
 *   아직 없다. 필요해지면 그때 State 태그를 상의해서 추가한다.
 *
 * [메시는 판정하지 않는다 — SlowZone 이 한다]
 *   PuddleMesh 는 시각 전용이라 콜리전이 없다. 밟는 판정은 별도 트리거 볼륨
 *   (SlowZone)이 전담한다 — AMovementTrap 과 같은 구조다.
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API APuddle : public AActor
{
	GENERATED_BODY()

public:
	APuddle();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 시각 전용. 콜리전 없음 — 밟는 판정은 SlowZone 이 한다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UStaticMeshComponent> PuddleMesh;

	/** 이동속도 감소가 적용되는 범위. PuddleMesh 발밑 크기에 맞춰 BP 에서 조정할 것 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UBoxComponent> SlowZone;

	/**
	 * 안에 있는 동안 적용할 속도 감소 효과. MovementSpeed 속성을 깎는 Infinite GE 로 만들 것 —
	 * Duration 이 있는 GE 를 쓰면 웅덩이 안에 오래 서 있을 때 효과가 먼저 끝나버린다.
	 * 나갈 때는 RemoveGameplayEffectFromSelf 로 직접 걷어내므로 지속시간은 필요 없다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hazard|Puddle")
	TSubclassOf<UGameplayEffect> SlowEffectClass;
};
