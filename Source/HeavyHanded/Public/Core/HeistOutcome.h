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
	/** 아무도 빠져나오지 못했다. 목표를 채웠는지는 보지 않는다 (기획서 2장 — 전멸은 실패) */
	Failure  UMETA(DisplayName = "Failure"),

	/** 최소 1인은 빠져나왔지만 목표 금액을 못 채웠다 */
	Partial  UMETA(DisplayName = "Partial"),

	/** 목표를 채웠고 최소 1인이 빠져나왔다 (기획서 2장 작업 성공) */
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
 * [여기서 보지 않는 것] "한 명이라도 빠져나왔는가" 를 어떻게 세는지는 호출부가 정한다.
 *   체포 명단으로 볼 수도 있고 승차 명단으로 볼 수도 있는데, 그건 이 규칙이
 *   알 바가 아니다 — 이 함수는 두 사실을 등급으로 옮기기만 한다.
 *
 * [분모가 '전원' 이 아니라 '최소 1인' 인 이유] 기획서 2장 승패 조건이 그렇다 —
 *   작업 성공은 "목표 금액 달성 + 최소 1인 승차" 이고, 실패는 "목표 미달 또는 전멸" 이다.
 *   미승차자에 대한 패널티는 등급이 아니라 체포(다음 작업 관전)로 이미 치른다.
 */
namespace HeistOutcome
{
	HEAVYHANDED_API EHeistOutcome Evaluate(bool bTargetReached, bool bAnyoneEscaped);

	/** 로그용. 열거형 값을 그대로 찍으면 숫자만 나와서 읽을 수 없다 */
	HEAVYHANDED_API const TCHAR* ToString(EHeistOutcome Outcome);
}
