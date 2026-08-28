#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"      // TSubclassOf<ALootBase> 를 값으로 받는다 — 전방 선언 불가

class AActor;
class ALootBase;

/**
 * UI 가 화면에 쓰는 문구를 만드는 공용 함수들.
 *
 * [왜 모아 두는가] 같은 것을 두 화면이 다르게 부르면 플레이어는 다른 물건으로 읽는다.
 *   상호작용 프롬프트가 "Vase" 라고 부른 것을 결과 화면이 "BP_Loot_Vase_C" 라고 쓰면
 *   같은 도자기라는 걸 알 수 없다. 금액 표기(`$3,750`)와 시간 표기(`6:05`)도 마찬가지다.
 *
 * [Shared/ 가 아니라 UI/ 인 이유] 여기 있는 것은 전부 "사람에게 어떻게 보일 것인가" 다.
 *   판정에 쓰이는 값이 아니므로 다른 시스템이 알 필요가 없다.
 *   Shared/ 는 여러 시스템이 같은 식을 써야 하는 것만 두는 자리다 (문서 01).
 */
namespace HeavyUIText
{
	/**
	 * 노획물 표시 이름. 월드에 있는 액터에서 얻는다.
	 *
	 * DT_LootCatalog 행이 지정돼 있으면 그 이름이고, 없으면 아래 규칙으로 만든다.
	 */
	HEAVYHANDED_API FText LootName(const AActor* LootActor);

	/**
	 * 노획물 표시 이름. 클래스만 있을 때 쓴다 (결과 화면의 적재 목록).
	 *
	 * [액터 버전과 결과가 다를 수 있다] 표시 이름은 액터가 만들어질 때 표에서 채워지므로
	 *   클래스 기본값(CDO)에는 비어 있다. 그래서 이쪽은 표의 행 이름을 대신 쓴다 —
	 *   행 이름은 표의 열쇠라서 사람이 읽을 만한 값인 경우가 많다.
	 */
	HEAVYHANDED_API FText LootName(TSubclassOf<ALootBase> LootClass);

	/** 초를 "6:05" 로. 초는 항상 두 자리다 — 자릿수가 오가면 글자 폭이 흔들린다 */
	HEAVYHANDED_API FText Duration(float Seconds);

	/** 금액을 "$3,750" 으로. 천 단위 구분은 로케일이 정한다 */
	HEAVYHANDED_API FText Money(int32 Amount);
}
