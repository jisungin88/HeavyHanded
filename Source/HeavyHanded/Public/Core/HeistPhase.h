#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HeistPhase.generated.h"

/**
 * 페이즈가 바뀐 이유.
 *
 * [왜 남기는가] "왜 도주 페이즈로 넘어갔는가" 는 그 순간에만 알 수 있다.
 *   경보가 터져서인지 시간이 다 돼서인지는 Escape 에 들어간 뒤로는 복원할 방법이 없다.
 *   결과 화면이 "경보 발각" 과 "시간 초과" 를 구분해서 보여줘야 하므로 전이와 함께 실어 나른다.
 */
UENUM(BlueprintType)
enum class EHeistPhaseReason : uint8
{
	/** 앞 페이즈의 시간이 다 됐다 */
	Scheduled   UMETA(DisplayName = "Scheduled"),

	/** 경보 100% — 경계도가 래치됐다 */
	Alarm       UMETA(DisplayName = "Alarm"),

	/** 생존 인원이 전부 밴에 탔다 */
	AllEscaped  UMETA(DisplayName = "All Escaped"),

	/** 치트로 넘겼다. 결과 집계에서 제외할 판단 근거가 된다 */
	Cheat       UMETA(DisplayName = "Cheat")
};

/**
 * 작업 레벨의 페이즈 순서와 전이 규칙.
 *
 * [왜 GameMode 밖에 있는가] 전이 규칙은 "누가 구동하는가" 와 무관한 도메인 지식이다.
 *   GameMode 안에 두면 서버 전용 객체가 있어야만 "Heist 다음이 뭐지" 를 물어볼 수 있고,
 *   HUD 나 자동화 테스트처럼 GameMode 를 못 잡는 쪽은 규칙을 다시 적게 된다.
 *
 * [단일 진리원] 전이는 GetOrder() 배열 하나에서 전부 나온다.
 *   페이즈를 추가·삭제할 때 고칠 곳이 그 배열 한 줄이다.
 *   전이표와 지속 시간표를 따로 두면 둘이 어긋나도 컴파일러가 잡아 주지 않는다.
 */
namespace HeistPhase
{
	/** 진행 순서. Prep → Heist → Escape → Result */
	HEAVYHANDED_API const TArray<FGameplayTag>& GetOrder();

	/** 순서상 몇 번째인가. 페이즈 태그가 아니면 INDEX_NONE */
	HEAVYHANDED_API int32 IndexOf(const FGameplayTag& Phase);

	/** 다음 페이즈. 마지막(Result)이거나 순서에 없는 태그면 무효 태그를 돌려준다 */
	HEAVYHANDED_API FGameplayTag GetNext(const FGameplayTag& Phase);

	/** 로그용. 열거형 값을 그대로 찍으면 숫자만 나와서 읽을 수 없다 */
	HEAVYHANDED_API const TCHAR* ToString(EHeistPhaseReason Reason);
}
