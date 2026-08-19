#include "Loot/LootHeavyComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Loot/LootBase.h"
#include "Loot/LootLog.h"
#include "Loot/LootSettings.h"

ULootHeavyComponent::ULootHeavyComponent()
{
	// 아직 매 프레임 볼 것이 없다. 거리 제약이 들어오는 C 단계에서 다시 판단한다.
	PrimaryComponentTick.bCanEverTick = false;
}

void ULootHeavyComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerLoot = Cast<ALootBase>(GetOwner());
	if (!IsValid(OwnerLoot))
	{
		// 행 이름·태그·메시를 전부 ALootBase 에서 읽으므로 다른 액터에는 붙을 수 없다.
		UE_LOG(LogLoot, Warning,
			TEXT("[%s] ULootHeavyComponent 는 ALootBase 에만 붙일 수 있다. 중량형 판정이 비활성화된다."),
			*GetNameSafe(GetOwner()));
		return;
	}

	// 이 컴포넌트가 붙어 있다는 것이 곧 "중량형" 이라는 선언이다.
	// 예전에는 ALootBase 가 WeightClass 를 보고 달았는데, 그 예외를 여기서 없앤다.
	OwnerLoot->AddLootTypeTag(HHTags::Loot_Type_Heavy);

	ResolveData();
}

void ULootHeavyComponent::ResolveData()
{
	if (bDataResolved)
	{
		return;
	}
	bDataResolved = true;

	if (!IsValid(OwnerLoot))
	{
		OwnerLoot = Cast<ALootBase>(GetOwner());
		if (!IsValid(OwnerLoot))
		{
			return;
		}
	}

	const FName RowName = OwnerLoot->GetLootRowName();

	// 표를 안 쓰는 노획물이다. BP 에 적힌 Data 를 그대로 쓴다 — 실험물용 폴백이다.
	if (RowName.IsNone())
	{
		return;
	}

	const FLootHeavyData* Row = ULootSettings::FindTraitRow<FLootHeavyData>(
		ULootSettings::Get()->HeavyTable, RowName, GetName());

	if (!Row)
	{
		// 컴포넌트가 붙었다는 것은 중량형으로 만들겠다는 선언인데 표에 행이 없다.
		// 기본값으로 돌아서 겉보기에는 멀쩡하지만, 그립 소켓 이름이 이 메시와 맞지 않으면
		// 두 사람이 같은 지점을 잡게 되고 2인 캐리의 의미가 사라진다.
		// 반대 방향(행은 있는데 컴포넌트가 없다)은 ALootBase 가 잡는다.
		UE_LOG(LogLoot, Warning,
			TEXT("[Loot:%s] ULootHeavyComponent 가 붙어 있는데 DT_LootHeavy 에 '%s' 행이 없다 "
				 "— 기본값으로 돈다. 표에 행을 추가하거나 Project Settings > Game > Loot 에서 "
				 "Heavy Table 이 지정돼 있는지 확인할 것"),
			*OwnerLoot->GetName(), *RowName.ToString());
		return;
	}

	Data = *Row;
}
