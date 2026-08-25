#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"

/**
 * 이 인스턴스가 권위 계산을 해도 되는가. **정의는 여기 하나뿐이어야 한다.**
 * GetOwnerRole() 만으로는 뚫린다 — 클라에서 로컬 스폰된 비복제 액터는 ROLE_Authority 라
 * 게이트를 그냥 통과한다. 그래서 클라 창 여부와 소유자 롤 두 조건을 모두 본다.
 */
FORCEINLINE bool HasServerAuthority(const UActorComponent* Component)
{
	if (!Component)
	{
		return false;
	}

	const UWorld* World = Component->GetWorld();
	if (!World || World->IsNetMode(NM_Client))
	{
		return false;
	}

	return Component->GetOwnerRole() == ROLE_Authority;
}

/** 월드 단위 판정. 서브시스템 · 콘솔 명령처럼 소유 액터가 없는 쪽이 쓴다 */
FORCEINLINE bool HasServerAuthority(const UWorld* World)
{
	return World && !World->IsNetMode(NM_Client);
}
