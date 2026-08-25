#pragma once

#include "CoreMinimal.h"
#include "HeistOutcome.generated.h"

/**
 * 한 판의 결과 등급. **값 순서에 의미가 있다** — Failure < Partial < Success 로 정렬돼 있어
 * 지급 기준선을 비교 연산 하나로 판정한다. 순서를 바꾸면 그 판정이 조용히 뒤집힌다.
 */
UENUM(BlueprintType)
enum class EHeistOutcome : uint8
{
	/** 아무도 빠져나오지 못했다. 목표를 채웠는지는 보지 않는다 (기획서 2장 — 전멸은 실패) */
	Failure  UMETA(DisplayName = "Failure"),

	/** 최소 1인은 빠져나왔지만 목표 금액을 못 채웠다 */
	Partial  UMETA(DisplayName = "Partial"),

	/** 목표를 채웠고 최소 1인이 빠져나왔다 (기획서 2장 작업 성공) */
	Success  UMETA(DisplayName = "Success")
};

/**
 * 결과 등급 판정. 월드를 모르는 순수 함수라 테스트가 직접 부른다.
 * 두 조건 조합이라 눈으로도 읽히지만 그래서 더 조용히 틀린다 — 한쪽을 빼먹어도 등급만
 * 한 칸 어긋나고 그것이 팀 골드 지급을 바꾼다. 세는 방법은 호출부가 정한다.
 */
namespace HeistOutcome
{
	HEAVYHANDED_API EHeistOutcome Evaluate(bool bTargetReached, bool bAnyoneEscaped);

	/** 로그용. 열거형 값을 그대로 찍으면 숫자만 나와서 읽을 수 없다 */
	HEAVYHANDED_API const TCHAR* ToString(EHeistOutcome Outcome);
}
