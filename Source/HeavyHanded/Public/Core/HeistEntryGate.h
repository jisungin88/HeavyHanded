#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"   // FGameplayTag 를 값으로 주고받는다

/** 진입점 판정 결과 */
enum class EHeistEntryDecision : uint8
{
	/** 은신처에서 고른 진입점이 이 레벨에 있다. 그대로 쓴다 */
	Selected,

	/** 고른 것이 없거나 이 레벨에 없는 태그다. 첫 번째 진입점으로 떨어진다 (경고 대상) */
	Fallback,

	/** 이 레벨에 진입점이 하나도 없다. 엔진 기본 스폰으로 넘긴다 (경고 대상) */
	None
};

/** 판정 결과와, 고른 것이 Available 의 몇 번째인가 */
struct FHeistEntryResolution
{
	EHeistEntryDecision Decision = EHeistEntryDecision::None;

	/** Available 배열의 인덱스. None 이면 INDEX_NONE */
	int32 Index = INDEX_NONE;
};

/**
 * "이 판을 어느 진입점에서 시작하는가" 판정.
 *
 * [왜 GameMode 밖으로 뺐는가] 이 판정이 틀려도 크래시가 나지 않는다.
 *   고른 진입점이 레벨에서 사라졌거나 태그를 오타 냈을 때 **조용히 엉뚱한 곳에서 시작**할 뿐이고,
 *   그건 "왜 자꾸 정문에서 시작하지?" 라는 형태로만 드러난다. 로그도 컴파일 경고도 없다.
 *
 *   레벨 디자이너가 진입점을 옮기거나 지우는 일은 계속 일어난다. 그때마다 은신처에 남아 있던
 *   선택은 이 레벨에 없는 태그가 된다 — 그 경로가 정확히 이 함수가 지키는 것이다.
 *
 *   월드·액터를 모르는 순수 함수로 두면 상황을 값으로 만들어 바로 검증할 수 있다.
 *   부수효과(액터 조회 · 로그 · 밴 배치)는 호출부인 AHeistGameMode 가 갖는다.
 *   HeistStartGate · HeistEscapeGate 와 같은 형태다.
 *
 * [Available 은 정렬돼 있어야 한다] 폴백이 마지막에 "첫 번째" 로 떨어지기 때문이다.
 *   액터 순회 순서는 보장되지 않으므로 호출부가 태그 이름으로 정렬해서 넘긴다.
 *   정렬하지 않으면 같은 레벨인데도 실행할 때마다 다른 곳에서 시작한다.
 *
 * [폴백은 두 단계다] 레벨이 지정한 기본 진입점 → 없으면 첫 번째.
 *   알파벳순 첫 번째만으로 두면 기본 진입점이 **태그 이름에 따라 정해진다** —
 *   Entry.Mansion.Alley 를 추가하는 것만으로 기본값이 정문에서 뒷골목으로 조용히 옮겨간다.
 *   레벨 디자이너가 명시적으로 고를 수 있어야 한다 (AHeistEntryPoint::bIsDefaultEntry).
 */
namespace HeistEntryGate
{
	/**
	 * @param Available     이 레벨의 진입점 태그. 태그 이름순으로 정렬돼 있어야 한다
	 * @param Selected      은신처에서 고른 것. 무효면 안 골랐다는 뜻이다
	 * @param DefaultIndex  레벨이 지정한 기본 진입점의 인덱스. 지정이 없으면 INDEX_NONE
	 */
	HEAVYHANDED_API FHeistEntryResolution Resolve(const TArray<FGameplayTag>& Available,
		const FGameplayTag& Selected, int32 DefaultIndex = INDEX_NONE);

	/** 로그용. 열거형 값을 그대로 찍으면 숫자만 나와서 읽을 수 없다 */
	HEAVYHANDED_API const TCHAR* ToString(EHeistEntryDecision Decision);
}
