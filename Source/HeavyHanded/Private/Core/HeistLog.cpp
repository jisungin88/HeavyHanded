#include "Core/HeistLog.h"

// 특정 클래스의 .cpp 가 아니라 여기서 정의한다.
// 코어 루프(GameMode · GameState)와 런 진행(서브시스템)이 함께 쓰는 카테고리라
// 어느 한 클래스에 얹어 두면 그 클래스 없이는 링크되지 않는다.
// LootLog 가 ALootBase.cpp 에 있는 것은 그쪽은 노획물 액터가 명확한 주인이기 때문이다.
DEFINE_LOG_CATEGORY(LogHeist);
