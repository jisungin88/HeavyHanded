#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Loot/LootTypes.h"              // FLootHeavyData 를 값으로 보유 — 전방 선언 불가
#include "LootHeavyComponent.generated.h"

class ALootBase;

/**
 * 중량형 노획물. 두 사람이 양 끝을 잡고 옮긴다. (기획서 5장 — 1인 시 속도 30%, 2인이면 100%)
 *
 * [왜 컴포넌트를 만드나 — 값만 달랐는데]
 *   중량형은 오랫동안 FLootPhysicsData 의 WeightClass 값으로만 존재했다. 고유 행동이
 *   없었으니 맞는 선택이었다. 그런데 2인 캐리가 들어오면서 중량형만의 행동이 생겼다 —
 *   두 번째 운반자를 기억하고, 두 그립 지점을 계산하고, 거리 제약을 푼다.
 *
 *   그래서 파손형·불안정형과 같은 모양으로 맞춘다. 이 컴포넌트가 붙어 있다는 것이
 *   곧 '중량형' 이라는 선언이고, 수치는 DT_LootHeavy 에서 같은 행 이름으로 온다.
 *   ALootBase 가 WeightClass 를 보고 태그를 달던 예외가 이걸로 없어진다.
 *
 * [지금 하는 일은 선언과 수치 보관까지다]
 *   두 번째 운반자 관리와 거리 제약은 B·C 단계에서 여기 붙는다.
 *   실제로 폰을 옮기는 것은 끝까지 플레이어 파트 몫이다 — 서버가 매 프레임 폰 위치를
 *   강제하면 CharacterMovementComponent 의 클라이언트 예측과 싸워서 캐릭터가 떨린다.
 *   이 컴포넌트는 그립 지점·거리·제약을 푸는 값까지만 제공한다.
 *
 * [경계] 이동 속도를 여기서 깎지 않는다. GetCarrySpeedMultiplier 로 값만 알린다.
 *   양쪽에서 적용하면 배율이 두 번 곱해진다.
 */
UCLASS(ClassGroup = (Loot), meta = (BlueprintSpawnableComponent))
class HEAVYHANDED_API ULootHeavyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULootHeavyComponent();

	/** 페널티 없이 옮기려면 필요한 인원. ALootBase::GetRequiredCarriers 가 이 값을 쓴다 */
	int32 GetRequiredCarriers() const { return Data.RequiredCarriers; }

	/** 두 사람이 각각 잡을 노획물 메시의 소켓 이름 */
	FName GetGripSocketA() const { return Data.GripSocketA; }
	FName GetGripSocketB() const { return Data.GripSocketB; }

protected:
	virtual void BeginPlay() override;

	/**
	 * 설계 수치. DT_LootHeavy 에서 노획물의 행 이름으로 찾아 채운다.
	 *
	 * 표에 행이 없으면 여기 적힌 값이 그대로 쓰인다 — 표에 안 올린 실험물용 폴백이다.
	 * 행이 있으면 ResolveData 가 덮어쓰므로 여기 값은 무시된다.
	 * 파손형·불안정형 컴포넌트와 같은 구조다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Heavy")
	FLootHeavyData Data;

private:
	/**
	 * DT_LootHeavy 에서 자기 행을 찾아 Data 를 채운다. 한 번만 실제로 돈다.
	 *
	 * 클라이언트에서도 부른다. 표 조회는 모든 머신에서 같은 답이 나오는 순수 계산이라
	 * 수치를 복제할 이유가 없다.
	 */
	void ResolveData();

	/** 소유 노획물. UPROPERTY 가 없으면 GC 가 회수한 뒤 엉뚱한 곳에서 크래시한다 */
	UPROPERTY()
	TObjectPtr<ALootBase> OwnerLoot;

	/** ResolveData 가 이미 돌았는가. 표 조회를 매번 반복하지 않기 위한 것이다 */
	bool bDataResolved = false;
};
