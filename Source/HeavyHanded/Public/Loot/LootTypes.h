#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"          // 부모 구조체 FTableRowBase — 전방 선언 불가
#include "Core/HeavyHandedTypes.h"     // FLootPhysicsData 를 값으로 보유
#include "LootTypes.generated.h"

/**
 * 노획물 한 종류의 설계값. DT_LootCatalog 의 한 행이다.
 *
 * [왜 DataAsset 이 아니라 DataTable 인가]
 *   기획자가 "도자기가 은촛대보다 비싼가" 를 한눈에 보고 한 자리에서 고쳐야 한다.
 *   DataAsset 은 노획물마다 파일이 하나씩 생겨서 비교하려면 창을 여러 개 열어야 한다.
 *   결정적인 이유는 CSV 다 — DataTable 은 표를 CSV 로 내보내고 되받을 수 있고,
 *   CSV 는 텍스트라 git 이 줄 단위로 diff 를 보여준다. uasset 은 병합이 안 되므로
 *   수치 조정이 잦은 데이터를 uasset 안에만 두면 두 사람이 만지는 순간 한쪽이 날아간다.
 *   소음 파트의 DT_NoiseProfiles 도 같은 이유로 DataTable 이다.
 *
 * [값만 담는다 — 어떤 특성인지는 여기서 정하지 않는다]
 *   파손형·불안정형은 컴포넌트를 붙이는 것이 곧 선언이다 (BP 조합).
 *   여기에 "파손형인가" 같은 칸을 만들면 컴포넌트는 없는데 표에는 파손형인 상태가
 *   만들어지고, 둘 중 무엇이 진실인지 알 수 없게 된다.
 *   중량형만은 예외로 Physics.WeightClass 로 결정된다 — 전용 컴포넌트가 없기 때문이다.
 *
 * [행 이름] RowName 이 곧 노획물 ID 다. Loot_Museum_Vase 처럼 장소_물건 순으로 적는다.
 */
USTRUCT(BlueprintType)
struct FLootDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 화면에 뜨는 이름. 상호작용 프롬프트와 정산 목록이 쓴다.
	 *
	 * FString 이 아니라 FText 인 이유는 현지화 때문이다. 게임은 한국어로 만들지만
	 * 나중에 영어를 넣을 때 FString 이면 문자열을 전부 찾아 바꿔야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FText DisplayName;

	/**
	 * 손상되지 않았을 때의 가치($).
	 *
	 * 기획서 목표 금액은 저택 $50,000 / 박물관 $120,000 / 은행 $250,000 이다.
	 * 한 장소의 노획물 값을 다 더한 것이 목표 금액보다 충분히 커야 선택의 여지가 생긴다.
	 * 표에서 세로로 훑으며 맞추라고 이 칸이 여기 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 BaseValue = 1000;

	/**
	 * 무게·운반·던지기·파손 임계값.
	 *
	 * 필드를 여기에 옮겨 적지 않고 기존 구조체를 통째로 품는다. 옮겨 적으면 같은 값이
	 * 두 벌이 되고, 한쪽만 고쳤을 때 컴파일도 통과해서 발견이 늦어진다.
	 *
	 * [주의] DamageImpulseThreshold 는 질량에 비례한다. MassKg 를 바꾸면 이 값도
	 *   같이 조정해야 한다 (10kg 기준 실측: 100cm 낙하 5031 / 150cm 6497 / 300cm 9622).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FLootPhysicsData Physics;
};
