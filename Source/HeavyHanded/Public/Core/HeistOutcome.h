#pragma once

#include "CoreMinimal.h"
#include "HeistOutcome.generated.h"

/**
 * 한 판의 결과 등급.
 *
 * [값 순서에 의미가 있다] Failure < Partial < Success 로 정렬돼 있어서
 *   "이 등급 이상이면 보상을 준다" 같은 판정을 비교 연산 하나로 할 수 있다
 *   (UHeistSettings::MinOutcomeForPayout). 순서를 바꾸면 그 판정이 조용히 뒤집힌다.
 */
UENUM(BlueprintType)
enum class EHeistOutcome : uint8
{
	/** 목표도 못 채웠고 누군가 잡혔다 */
	Failure  UMETA(DisplayName = "Failure"),

	/** 둘 중 하나만 됐다 — 돈은 모자라거나, 사람을 두고 왔거나 */
	Partial  UMETA(DisplayName = "Partial"),

	/** 목표를 채웠고 전원이 무사히 빠져나왔다 */
	Success  UMETA(DisplayName = "Success")
};

/**
 * 결과 등급 판정.
 *
 * [왜 GameState 밖에 있는가] 규칙이 두 조건의 조합이라 눈으로도 읽히지만, 그래서 더
 *   조용히 틀린다 — 어느 한쪽을 빼먹어도 컴파일도 되고 크래시도 안 나고 등급만 한 칸 어긋난다.
 *   그 어긋남은 팀 골드 지급 여부까지 바꾼다.
 *
 *   HeistStartGate · HeistEscapeGate 와 같은 형태다. 월드를 모르는 순수 함수로 두고
 *   Private/Tests/HeistStartGateTest.cpp 가 직접 부른다.
 *
 * [여기서 보지 않는 것] "전원 탈출인가" 를 어떻게 세는지는 호출부가 정한다.
 *   체포 명단이 비었는지로 볼 수도 있고 승차 명단으로 볼 수도 있는데, 그건 이 규칙이
 *   알 바가 아니다 — 이 함수는 두 사실을 등급으로 옮기기만 한다.
 */
namespace HeistOutcome
{
	HEAVYHANDED_API EHeistOutcome Evaluate(bool bTargetReached, bool bEveryoneEscaped);

	/** 로그용. 열거형 값을 그대로 찍으면 숫자만 나와서 읽을 수 없다 */
	HEAVYHANDED_API const TCHAR* ToString(EHeistOutcome Outcome);
}
