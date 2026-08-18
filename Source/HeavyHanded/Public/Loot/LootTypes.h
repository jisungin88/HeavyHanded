#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"          // 부모 구조체 FTableRowBase — 전방 선언 불가
#include "Core/HeavyHandedTypes.h"     // FLootPhysicsData 를 값으로 보유
#include "LootTypes.generated.h"

/**
 * 불안정형 노획물의 설계값. (기획서 5장 — 기울기 60도 초과 시 내용물 유출)
 *
 * ULootStabilityComponent 가 아니라 ALootBase 가 들고 있고 컴포넌트는 읽기만 한다.
 * FLootPhysicsData 와 같은 구조다 — 값은 데이터, 행동은 컴포넌트.
 *
 * [왜 컴포넌트에서 뺐나] 이 값들은 노획물마다 달라야 하는 설계 수치다.
 *   동전 자루와 물통은 새기 시작하는 각도도, 한 번에 잃는 비율도 달라야 한다.
 *   컴포넌트 안에 있으면 기획자가 BP 를 열어야 하고, BP 는 병합이 안 된다.
 *   파손형 수치(DamageImpulseThreshold, MaxImpactCount)는 이미 표에 있는데
 *   불안정형만 BP 에 남아 있는 비대칭도 여기서 없앤다.
 */
USTRUCT(BlueprintType)
struct FLootStabilityData
{
	GENERATED_BODY()

	/** 이 각도를 넘으면 샌다(도). 수직에서 벗어난 각도이므로 90 이면 완전히 누운 상태다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
		meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float SpillTiltAngle = 60.f;

	/**
	 * 이 속도 이하로 움직이면 기울기가 쌓이지 않는다(cm/s).
	 * 걷기는 안전하고 뛰면 쌓이는 지점에 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
		meta = (ClampMin = "0.0"))
	float SafeCarrySpeed = 200.f;

	/**
	 * 안전 속도 초과분 1cm/s 당 초당 쌓이는 기울기(도).
	 *
	 * 속도와 시간이 둘 다 들어가는 것이 핵심이다. 얼마나 빠른지(초과분)와
	 * 얼마나 오래 그랬는지(누적)가 같이 반영돼야 "조금만 더 뛸까" 하는 판단이 생긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
		meta = (ClampMin = "0.0"))
	float TiltGainPerSpeed = 0.06f;

	/** 안전 속도 이하일 때 되돌아오는 속도(도/초). 멈춰서 숨 고르면 회복된다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
		meta = (ClampMin = "0.0"))
	float TiltRecoverRate = 25.f;

	/**
	 * 기우는 방향이 이동 방향을 따라가는 속도(도/초).
	 *
	 * 방향을 즉시 바꾸면 좌우로 왔다 갔다 할 때 물건이 순간이동하듯 꺾인다.
	 * 내용물이 쏠렸다가 반대로 쏠리는 데도 시간이 걸린다고 보면 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability|Carry",
		meta = (ClampMin = "0.0"))
	float TiltDirectionTurnRate = 180.f;

	/**
	 * 놓인 상태에서 이 시간 이상 연속으로 기울어져 있어야 샌다(초).
	 *
	 * 던지면 ThrowSpinSpeed 로 회전하면서 날아가기 때문에, 순간 각도만 보면
	 * 공중에서 새 버린다. 시간으로 한 겹 걸러야 '넘어진 것'과 '회전 중인 것'이 갈린다.
	 * 굴러가는 중에는 계속 기울어져 있으므로 그대로 통과한다 — 의도한 동작이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
		meta = (ClampMin = "0.0"))
	float TiltGraceSeconds = 0.5f;

	/** 기울어져 있는 동안 이 주기로 계속 샌다(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
		meta = (ClampMin = "0.1"))
	float SpillIntervalSeconds = 2.f;

	/** 1회 유출당 깎이는 가치 비율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpillValueLossRatio = 0.15f;

	/**
	 * 설계 가치 대비 이 비율 밑으로는 깎이지 않는다.
	 * 가치 0 은 파손형의 몫이다. 불안정형까지 0 이 되면 두 특성이 같아진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Stability",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinValueRatio = 0.2f;
};

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

	/**
	 * 불안정형 수치. ULootStabilityComponent 를 붙인 노획물만 쓴다.
	 *
	 * 컴포넌트가 없는 행에도 칸은 남아 있는데, 채워도 아무 일이 일어나지 않는다.
	 * 특성을 표에서 정하지 않는다는 원칙 때문이다 — 컴포넌트를 붙이는 것이 곧 선언이고,
	 * 여기서 "불안정형인가" 를 또 정하면 둘 중 무엇이 진실인지 알 수 없게 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FLootStabilityData Stability;
};
