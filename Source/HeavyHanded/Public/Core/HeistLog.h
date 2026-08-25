#pragma once

#include "CoreMinimal.h"

/**
 * 코어 루프와 런 진행의 로그 카테고리. 둘을 가르지 않는 것은 같은 이야기의 앞뒤라서다.
 * static 이 아니라 EXTERN 인 것은 GameMode · GameState · 적재존이 한 사건을 나눠 처리해서,
 * "이 판이 왜 Escape 로 넘어갔나" 를 한 필터로 따라가야 하기 때문이다.
 */
DECLARE_LOG_CATEGORY_EXTERN(LogHeist, Log, All);
