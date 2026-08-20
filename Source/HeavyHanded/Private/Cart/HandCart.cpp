#include "Cart/HandCart.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Loot/LootBase.h"
#include "Loot/LootLog.h"
#include "Noise/NoiseSubsystem.h"        // 걸러낸 충돌만 직접 발행한다

namespace
{
	/**
	 * 카트 몸체의 콜리전 프로파일.
	 *
	 * 노획물의 시뮬레이션 프로파일을 그대로 쓴다. 이 프로파일은 Camera 와 NoiseOcclusion 만
	 * 무시하고 나머지를 전부 Block 하는데, 카트에 필요한 것이 정확히 그것이다 —
	 * 특히 Pawn 을 막아야 벽에 걸린 카트가 미는 사람도 세울 수 있다.
	 *
	 * 오브젝트 타입이 "Loot" 로 잡히는 것이 걸리지만, 지금 노획물을 고르는 코드는
	 * 콜리전 채널이 아니라 Loot.Type 태그로 판정한다(GAB_Interact). 카트에는 그 태그가 없으니
	 * 집기 대상으로 잡히지 않는다. 채널로 거르는 코드가 생기면 그때 전용 프로파일을 만든다.
	 */
	static const FName CartCollisionProfile(TEXT("Loot"));

	/**
	 * 적재 판정 볼륨의 프로파일. 노획물만 Overlap 하고 나머지는 전부 Ignore 한다.
	 *
	 * 엔진 기본 프로파일(OverlapAllDynamic 등)을 쓰면 안 된다. Loot 은 나중에 추가한
	 * 커스텀 채널이라 엔진 프로파일에는 응답이 적혀 있지 않고, 그러면 채널 기본값인
	 * Block 이 적용돼 오버랩 이벤트가 한 번도 오지 않는다. (Config/DefaultEngine.ini)
	 */
	static const FName CartLoadZoneProfile(TEXT("CartLoadZone"));

	/** 적재면 위 공간의 기본 크기(cm). 임시 메시 기준이라 실제 카트가 오면 BP 에서 맞춘다 */
	static const FVector DefaultLoadExtent(60.f, 40.f, 30.f);
	static const FVector DefaultLoadOffset(0.f, 0.f, 60.f);
}

AHandCart::AHandCart()
{
	// 매 프레임 할 일이 아직 없다. 끌기가 들어오면 그때 다시 판단한다.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// 물리 물체는 서버 스냅샷 간격이 곧 클라이언트가 보는 끊김이 된다.
	// 노획물과 같은 값으로 맞춘다.
	NetUpdateFrequency = 60.f;
	MinNetUpdateFrequency = 20.f;

	CartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartMesh"));
	SetRootComponent(CartMesh);
	CartMesh->SetCollisionProfileName(CartCollisionProfile);
	CartMesh->SetSimulatePhysics(true);

	// 이게 없으면 OnComponentHit 이 아예 오지 않는다 — 벽에 박아도 소리가 안 난다.
	CartMesh->SetNotifyRigidBodyCollision(true);

	// 앞뒤·좌우 회전을 잠근다. Z(요)만 남겨서 방향 전환은 되고 뒤집히지는 않게 한다.
	// SixDOF 로 놓지 않으면 개별 축 잠금이 무시된다.
	CartMesh->BodyInstance.DOFMode = EDOFMode::SixDOF;
	CartMesh->BodyInstance.bLockXRotation = true;
	CartMesh->BodyInstance.bLockYRotation = true;

	LoadVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("LoadVolume"));
	LoadVolume->SetupAttachment(CartMesh);
	LoadVolume->SetBoxExtent(DefaultLoadExtent);
	LoadVolume->SetRelativeLocation(DefaultLoadOffset);

	// 판정용 볼륨이라 물리에는 관여하지 않는다. 물건을 담아 두는 것은 카트 메시의 턱이 한다.
	LoadVolume->SetCollisionProfileName(CartLoadZoneProfile);
	LoadVolume->SetGenerateOverlapEvents(true);

	// 판정 볼륨이지 적재함 벽이 아니다. 게임 화면에 보이면 안 된다.
	LoadVolume->SetHiddenInGame(true);

}

void AHandCart::BeginPlay()
{
	Super::BeginPlay();

	// 메시가 없으면 물리 바디가 안 만들어진다. 그러면 밀어도 안 움직이고 벽도 못 막는데,
	// 화면에는 BP 에서 따로 붙인 다른 메시가 보여서 "카트는 있는데 왜 안 밀리지" 가 된다.
	// BP 에서 Cube 컴포넌트를 새로 추가하는 대신 CartMesh 에 메시를 지정해야 한다.
	if (!CartMesh->GetStaticMesh())
	{
		UE_LOG(LogLoot, Warning,
			TEXT("[Cart:%s] CartMesh 에 스태틱 메시가 없다 — 물리 바디가 만들어지지 않아 "
				 "밀리지도 않고 벽에 막히지도 않는다. BP 에 메시 컴포넌트를 새로 추가하지 말고 "
				 "CartMesh 의 Static Mesh 칸에 지정할 것"),
			*GetName());
	}
	else if (!CartMesh->IsSimulatingPhysics())
	{
		// 루트가 Static/Stationary 면 물리가 안 돈다. 에디터에서 실수로 바꾸는 일이 잦다.
		UE_LOG(LogLoot, Warning,
			TEXT("[Cart:%s] CartMesh 가 물리 시뮬레이션을 하지 않는다 — Mobility 가 Movable 인지, "
				 "Simulate Physics 가 켜져 있는지 확인할 것"),
			*GetName());
	}

	CartMesh->SetMassOverrideInKg(NAME_None, MassKg, true);

	if (!bLockTipping)
	{
		// BP 에서 껐으면 생성자에서 걸어 둔 잠금을 푼다.
		CartMesh->BodyInstance.bLockXRotation = false;
		CartMesh->BodyInstance.bLockYRotation = false;
		CartMesh->BodyInstance.SetDOFLock(EDOFMode::SixDOF);
	}

	// 클라이언트는 예측하지 않고 서버 스냅샷을 향해 보간만 한다. 노획물과 같은 정책이다.
	SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);

	// 적재 판정은 서버가 정한다. 클라이언트에서 각자 세면 사람마다 다른 답이 나온다.
	if (HasAuthority())
	{
		LoadVolume->OnComponentBeginOverlap.AddDynamic(this, &AHandCart::HandleLoadBeginOverlap);
		LoadVolume->OnComponentEndOverlap.AddDynamic(this, &AHandCart::HandleLoadEndOverlap);
		CartMesh->OnComponentHit.AddDynamic(this, &AHandCart::HandleCartHit);

		// 레벨에 물건이 이미 실린 채로 배치돼 있을 수 있다.
		// 그 경우 BeginOverlap 이 오지 않으므로 여기서 한 번 훑는다.
		TArray<AActor*> Overlapping;
		LoadVolume->GetOverlappingActors(Overlapping, ALootBase::StaticClass());
		for (AActor* Actor : Overlapping)
		{
			ContainLoot(Cast<ALootBase>(Actor));
		}
	}
}

void AHandCart::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 카트가 사라지는데 노획물이 조용한 채로 남으면, 그 물건은 남은 판 내내 소리를 안 낸다.
	// 소음 억제는 카트가 걸어 준 것이라 카트가 풀어 주고 나가야 한다.
	for (int32 Index = ContainedLoot.Num() - 1; Index >= 0; --Index)
	{
		if (ALootBase* Loot = ContainedLoot[Index].Get())
		{
			Loot->SetContainingCart(nullptr);
		}
	}
	ContainedLoot.Reset();

	Super::EndPlay(EndPlayReason);
}

bool AHandCart::IsContaining(const ALootBase* Loot) const
{
	return Loot && ContainedLoot.Contains(Loot);
}

void AHandCart::ContainLoot(ALootBase* Loot)
{
	if (!HasAuthority() || !IsValid(Loot) || ContainedLoot.Contains(Loot))
	{
		return;
	}

	// 사람이 든 채로 카트 위를 지나가는 것은 적재가 아니다.
	// 손에서 놓아야 실린 것으로 센다 — 들고 서 있기만 해도 조용해지면 카트가 필요 없어진다.
	if (Loot->GetPrimaryCarrier() != nullptr)
	{
		return;
	}

	// 이미 다른 카트에 실려 있으면 그쪽에서 먼저 뺀다. 두 카트가 같은 물건을 들고 있으면
	// 나중에 어느 쪽이 소음 억제를 풀어야 하는지가 어긋난다.
	if (AHandCart* Previous = Loot->GetContainingCart())
	{
		if (Previous != this)
		{
			Previous->ReleaseLoot(Loot);
		}
	}

	ContainedLoot.Add(Loot);
	Loot->SetContainingCart(this);

	UE_LOG(LogLoot, Verbose, TEXT("[Cart:%s] %s 적재 (총 %d개)"),
		*GetName(), *Loot->GetName(), ContainedLoot.Num());
}

void AHandCart::ReleaseLoot(ALootBase* Loot)
{
	if (!HasAuthority() || !Loot)
	{
		return;
	}

	if (ContainedLoot.Remove(Loot) > 0)
	{
		Loot->SetContainingCart(nullptr);

		UE_LOG(LogLoot, Verbose, TEXT("[Cart:%s] %s 이탈 (총 %d개)"),
			*GetName(), *Loot->GetName(), ContainedLoot.Num());
	}
}

void AHandCart::HandleLoadBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	ContainLoot(Cast<ALootBase>(OtherActor));
}

void AHandCart::HandleLoadEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	ReleaseLoot(Cast<ALootBase>(OtherActor));
}

bool AHandCart::ShouldReportHitAsNoise(const AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	// 실려 있는 물건이 적재함 안에서 부딪히는 것은 소음이 아니다.
	// 이걸 안 거르면 물건 쪽 소음을 아무리 막아도 카트가 대신 시끄러워서,
	// 소음을 줄이려고 산 장비가 소음 발생기가 된다.
	if (const ALootBase* Loot = Cast<ALootBase>(OtherActor))
	{
		if (Loot->GetContainingCart() == this)
		{
			return false;
		}
	}

	// 사람이 몸으로 미는 것도 소음이 아니다. 밀고 다니는 내내 접촉이 이어져서,
	// 이걸 소리로 치면 카트를 쓰는 것 자체가 시끄러워진다.
	if (OtherActor->IsA<APawn>())
	{
		return false;
	}

	return true;
}

void AHandCart::HandleCartHit(UPrimitiveComponent* /*HitComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, FVector NormalImpulse, const FHitResult& Hit)
{
	// 클라이언트의 물리 충돌은 신뢰하지 않는다. 시뮬레이션 결과가 머신마다 다르다.
	const UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !ShouldReportHitAsNoise(OtherActor))
	{
		return;
	}

	const float ImpulseMagnitude = NormalImpulse.Size();

	// [1겹] 임계값 미만 무시. 벽을 스치거나 바닥에 얹혀 있는 접촉까지 전부 OnHit 으로 온다.
	if (ImpulseMagnitude < NoiseImpulseThreshold)
	{
		return;
	}

	// [2겹] 같은 대상에 대한 짧은 시간 내 재발행 차단. 임계값만으로는 못 막는다 —
	// 벽에 한 번 박아도 강한 접촉이 연달아 여러 번 잡힌다. (ALootBase 와 같은 구조)
	const float Now = World->GetTimeSeconds();
	const TWeakObjectPtr<const AActor> Key(OtherActor);
	if (const float* Last = RecentNoiseTimes.Find(Key))
	{
		if (Now - *Last < NoiseDebounceSeconds)
		{
			return;
		}
	}
	RecentNoiseTimes.Add(Key, Now);

	// 키가 죽은 항목은 여기서 정리한다. 안 하면 맵이 계속 자란다.
	for (auto It = RecentNoiseTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now - It.Value() > NoiseDebounceSeconds * 4.f)
		{
			It.RemoveCurrent();
		}
	}

	UNoiseSubsystem* Noise = UNoiseSubsystem::Get(this);
	if (!Noise)
	{
		return;
	}

	// 세기를 0~1 로 정규화해서 넘긴다. 실제 크기·반경·경계도는 DT_NoiseProfiles 가 정한다 —
	// 여기서는 "얼마나 세게 부딪혔나" 만 알린다.
	//
	// 아직 카트 전용 행이 없어서 Noise.Loot.Impact 를 쓴다. 카트가 벽에 박는 소리를
	// 따로 튜닝하고 싶어지면 소음 파트에 행 추가를 요청할 것.
	// 굴러가는 지속음은 넣지 않기로 했다 (2026-08-20).
	const float LoudnessScale = FMath::Clamp(ImpulseMagnitude / NoiseFullImpulse, 0.f, 1.f);
	Noise->ReportNoise(HHTags::Noise_Loot_Impact, Hit.ImpactPoint, LoudnessScale, this);

	UE_LOG(LogLoot, Verbose, TEXT("[Cart:%s] 충돌 소음 %.2f (%s, 세기 %.0f)"),
		*GetName(), LoudnessScale, *GetNameSafe(OtherActor), ImpulseMagnitude);
}
