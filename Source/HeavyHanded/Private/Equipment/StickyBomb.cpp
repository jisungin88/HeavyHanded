#include "Equipment/StickyBomb.h"

#include "Core/HeavyHandedGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Loot/LootLog.h"

AStickyBomb::AStickyBomb()
{
	EquipmentTag = HHTags::Equipment_StickyBomb;

	// 던진 곳에 붙는다. 이것이 '점착' 이고, 미끼·EMP 와 갈리는 지점이다.
	bAttachOnImpact = true;

	// 붙은 뒤 퓨즈가 돈다. 붙자마자 터지면 던진 사람이 같이 휘말린다.
	ActivationMode = EEquipmentActivation::AfterDelay;
	ActivationDelay = 3.f;

	// 폭발은 순간적이다. 발동과 동시에 Spent 로 간다.
	EffectDuration = 0.f;

	// 던져서 정확히 붙여야 하는 물건이라 곧게 날아가야 한다.
	// 노획물 기본값(900 / 0.25)보다 빠르고 포물선이 얕다.
	ThrowParams.Speed = 1200.f;
	ThrowParams.UpwardRatio = 0.12f;

	// 붙는 물건이라 회전은 방해만 된다. 굴러가서 엉뚱한 면에 붙는다.
	ThrowParams.SpinSpeed = 0.f;
}

void AStickyBomb::OnActivated()
{
	Super::OnActivated();

	// 실제 파괴는 서버가 판정한다. 연출은 베이스가 모든 머신에서 이미 처리했다.
	if (!HasAuthority())
	{
		return;
	}

	// TODO(3번 작업): 반경 안의 ALargeSafe 문을 파괴한다.
	//   금고가 아직 없으므로 지금은 반경만 알린다. 폭탄이 금고를 직접 아는 대신
	//   인터페이스로 알릴지는 금고를 만들 때 정한다 — 반응할 대상이 하나뿐인데
	//   인터페이스를 먼저 파면 쓰이지 않는 추상이 하나 남는다.

	UE_LOG(LogLoot, Log, TEXT("[StickyBomb:%s] 폭발 — 반경 %.0f"), *GetName(), BlastRadius);

#if ENABLE_DRAW_DEBUG
	// 붙인 자리와 반경이 맞는지 눈으로 확인한다. 금고를 붙이면 지운다.
	if (const UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, GetActorLocation(), BlastRadius, 16, FColor::Orange, false, 3.f, 0, 2.f);
	}
#endif
}
