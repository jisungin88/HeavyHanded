#pragma once

#include "CoreMinimal.h"

/**
 * 환경 방해 요소 공통 로그 카테고리 (기획서 6장).
 *
 * [왜 헤더에 EXTERN 인가] 기본은 .cpp 안의 DEFINE_LOG_CATEGORY_STATIC 이다 (문서 07 테스트).
 *   ABreakableWall 하나뿐일 때는 그 파일 안에 static 으로 두었는데, AMovementTrap 이
 *   추가되면서 두 파일이 같은 카테고리를 공유하게 됐다 — LogLoot/LogGuardAI 와 같은 이유로
 *   여기로 옮긴다.
 *
 * [무엇을 찍나] 이 시스템도 실패해도 예외가 나지 않는다 — 메시가 비었거나 이펙트가
 * 없으면 "부서져도/걸려도 아무 일도 안 일어난다" 로만 드러난다. 조용히 return 하는
 * 지점마다 이유를 남긴다.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogHazard, Log, All);
