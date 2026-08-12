#pragma once

#include "CoreMinimal.h"

/**
 * BB_Guards 의 Blackboard 키 이름.
 *
 * 컨트롤러와 BT 노드가 각자 TEXT("...") 리터럴을 쓰고 있었다. Blackboard 는
 * 없는 키를 조회해도 예외 없이 기본값을 돌려주므로, 오타 하나가 로그도 없이
 * "조건이 항상 거짓"으로만 나타난다. 키 이름은 여기 한 곳에서만 정의한다.
 *
 * 여기 없는 키(SelfActor / IsInAttackRange / WanderLocation /
 * HasLineOfFireOnTarget)는 BB_Guards 에는 남아 있지만 코드에서 쓰지 않는다.
 */
namespace GuardAIKeys
{
	extern const FName TargetActor;
	extern const FName LastKnownLocation;
	extern const FName PatrolLocation;
	extern const FName CanSeeTarget;
	extern const FName SoundTargetActor;
	extern const FName DetectionGauge;
	extern const FName InvestigateLocation;
	extern const FName SearchStartTime;
	extern const FName LastSeenTime;
}
