#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"

/**
 * 이 인스턴스가 권위 계산(소음 발행 · 경계도 · 인지 게이지)을 해도 되는가.
 *
 * 정의가 여러 벌이 되지 않게 여기 하나만 둔다. 예전에는 컴포넌트 셋이 각자
 * HasNoiseAuthority() / HasAlertAuthority() 를 들고 있었고, 서브시스템은 또 다른 기준을 썼다 —
 * 컴포넌트는 GetOwnerRole(), 서브시스템은 NetMode.
 *
 * 그 둘은 같은 뜻이 아니다. 클라에서 로컬 스폰된 비복제 액터는 GetOwnerRole() 이
 * ROLE_Authority 라 컴포넌트 게이트를 그냥 통과한다. 지금까지 사고가 안 난 것은
 * 서브시스템 게이트가 뒤에서 한 번 더 막아줬기 때문이지, 의도한 안전이 아니었다.
 *
 * 그래서 두 조건을 모두 본다 — 클라 창이면 무조건 아니고, 그 외에는 소유자 롤을 따른다.
 *
 * 소음 전용이 아니라 서버 권위 일반이라 Shared 에 둔다.
 * (경계도 컴포넌트가 HasNoiseAuthority() 를 부르는 것은 이름이 어긋났다)
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
