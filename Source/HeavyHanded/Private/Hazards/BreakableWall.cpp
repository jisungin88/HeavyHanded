#include "Hazards/BreakableWall.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"        // TActorIterator — 디버그 치트(hh.Hazard.BreakWall)에서만 쓴다
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// AMovementTrap 이 추가되며 이 카테고리를 두 파일이 공유하게 돼 HazardLog.h 로 옮겼다.
DEFINE_LOG_CATEGORY(LogHazard);

ABreakableWall::ABreakableWall()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	// 벽은 움직이지 않는다. 위치를 계속 보내 봐야 대역폭만 쓴다 (AVaultDoor 와 동일 사유)
	SetReplicateMovement(false);

	// 레벨에 몇 개 없고 지속 시간 내내 상태가 중요한 액터라 항상 관련 액터로 둔다.
	// AVaultDoor::bAlwaysRelevant 와 동일 사유 — 늦게 들어온 관련성 때문에
	// 이미 끝난 파괴 연출이 다시 재생되는 것을 막는다.
	bAlwaysRelevant = true;

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	SetRootComponent(WallMesh);
	WallMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// 부서지면 사라지므로 Static 이면 그림자가 라이트맵에 구워져 남는다 (AVaultDoor::DoorLid 와 동일 사유)
	WallMesh->SetMobility(EComponentMobility::Movable);
}

void ABreakableWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABreakableWall, bIsBroken);
}

void ABreakableWall::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "부딪혔는데/터뜨렸는데 아무 일도 안 일어난다" 하나로만 드러난다.
	// 원인까지 가는 데 오래 걸리므로 켤 때 미리 말해 둔다 (AVaultDoor::BeginPlay 와 동일 사유).
	if (!IsValid(WallMesh) || !WallMesh->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[BreakableWall:%s] WallMesh 에 메시가 없다 — 부숴도 사라질 것이 없다"), *GetName());
	}

	if (!IsValid(BreakEffect))
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[BreakableWall:%s] BreakEffect 가 비어 있다 — 벽이 소리 없이 증발한다"), *GetName());
	}
}

bool ABreakableWall::TryBreak(const AActor* Breacher, const FVector& ImpactLocation, float ImpactRadius)
{
	// Server RPC 는 요청일 뿐이고 판정은 서버가 한다. 여기가 그 판정 자리다 (AVaultDoor 와 동일 사유)
	if (!HasAuthority() || bIsBroken)
	{
		return false;
	}

	if (!IsValid(WallMesh))
	{
		return false;
	}

	// ImpactRadius 가 0 이면 직접 타격(브루트 돌진)이다 — 닿았다는 사실 자체가 호출 조건이라
	// 거리 판정이 필요 없다.
	if (ImpactRadius > 0.f)
	{
		FVector ClosestPoint = FVector::ZeroVector;
		float Distance = WallMesh->GetClosestPointOnCollision(ImpactLocation, ClosestPoint);

		if (Distance < 0.f)
		{
			// 단순 콜리전이 없으면 표면을 잴 수 없다. 바운드 구로 대신한다 (AVaultDoor::TryBreach 와 동일 사유)
			Distance = FMath::Max(0.f,
				FVector::Dist(ImpactLocation, WallMesh->Bounds.Origin) - WallMesh->Bounds.SphereRadius);
		}

		// 폭발이 벽까지 닿아야 하고(ImpactRadius), 벽이 인정하는 거리여야 한다(BreakTolerance).
		// 둘 중 엄한 쪽이 이긴다 (AVaultDoor::TryBreach 와 동일 사유)
		if (Distance > FMath::Min(BreakTolerance, ImpactRadius))
		{
			UE_LOG(LogHazard, Log,
				TEXT("[BreakableWall:%s] 충격이 범위를 벗어났다 — 부서지지 않는다"), *GetName());
			return false;
		}
	}

	bIsBroken = true;

	// 서버에서 직접 대입하면 RepNotify 가 안 불린다. 손으로 불러야 호스트 화면에서도 부서진다
	// (AVaultDoor::TryBreach 와 동일 사유)
	OnRep_bIsBroken();

	UE_LOG(LogHazard, Log, TEXT("[BreakableWall:%s] 파괴 — 원인 %s"),
		*GetName(), Breacher ? *Breacher->GetName() : TEXT("Unknown"));
	return true;
}

void ABreakableWall::OnRep_bIsBroken()
{
	if (bIsBroken)
	{
		ApplyBreak();
	}
}

void ABreakableWall::ApplyBreak()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 피벗이 아니라 바운즈 중심이다 (AVaultDoor::ApplyBreach 와 동일 사유)
	const FVector WallCenter = IsValid(WallMesh) ? WallMesh->Bounds.Origin : GetActorLocation();
	const FVector EffectLocation = WallCenter + GetActorForwardVector() * BreakEffectForwardOffset;

	// 데디케이티드 서버는 화면도 스피커도 없다. 리슨 서버의 호스트는 클라이언트이기도 하므로
	// 여기 걸리지 않는다 (AVaultDoor::ApplyBreach 와 동일 사유)
	if (World->GetNetMode() != NM_DedicatedServer)
	{
		if (IsValid(BreakEffect))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, BreakEffect,
				EffectLocation, GetActorRotation(), FVector(BreakEffectScale));
		}

		if (IsValid(BreakSound))
		{
			UGameplayStatics::PlaySoundAtLocation(World, BreakSound, EffectLocation);
		}
	}

	// 벽 지우기는 데디케이티드 서버에서도 해야 한다 — 연출이 아니라 콜리전이다.
	// 안 지우면 서버에서만 자리가 막혀 있어서 클라이언트가 들어가려다 되밀린다 (AVaultDoor 와 동일 사유)
	if (WallHideDelay > 0.f)
	{
		World->GetTimerManager().SetTimer(HideTimer, this, &ABreakableWall::HideWall, WallHideDelay, false);
	}
	else
	{
		HideWall();
	}
}

void ABreakableWall::HideWall()
{
	if (!IsValid(WallMesh))
	{
		return;
	}

	// 장식이 벽 아래에 붙어 있을 수 있다. 전파를 켜야 같이 사라진다 (AVaultDoor::HideLid 와 동일 사유)
	WallMesh->SetVisibility(false, /*bPropagateToChildren=*/true);

	// 콜리전·내비게이션에는 전파 인자가 없어서 직접 훑는다
	TArray<USceneComponent*> Descendants;
	WallMesh->GetChildrenComponents(/*bIncludeAllDescendants=*/true, Descendants);
	Descendants.Add(WallMesh);

	for (USceneComponent* Component : Descendants)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!IsValid(Primitive))
		{
			continue;
		}

		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 경비가 뚫린 자리로 지나갈 수 있어야 한다. 내비메시가 동적일 때만 실제로 갱신되고,
		// 구워 둔 것이라면 아무 일도 일어나지 않는다 — 그때는 AI 파트에 갱신 방식을 물어야 한다
		// (AVaultDoor::HideLid 와 동일 사유)
		Primitive->SetCanEverAffectNavigation(false);
	}
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 점착 폭탄/브루트 돌진이 아직 이 벽을 모른다(StickyBomb.cpp 연결은
// 물리·아이템 담당 파일이라 별도 협의 후 진행). 그 전까지 파괴 판정·연출·복제만
// 먼저 검증하기 위한 치트. 쉬핑 빌드에서는 코드째 빠진다.
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

static void HazardBreakWallCommand(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (World->IsNetMode(NM_Client))
	{
		UE_LOG(LogHazard, Warning, TEXT("파괴는 서버 권위입니다. 클라이언트에서는 실행되지 않습니다."));
		return;
	}

	int32 BrokenCount = 0;

	for (TActorIterator<ABreakableWall> It(World); It; ++It)
	{
		// ImpactRadius 0 — 브루트 돌진과 같은 "직접 타격" 취급으로 거리 판정 없이 부순다
		if (It->TryBreak(nullptr, It->GetActorLocation(), 0.f))
		{
			++BrokenCount;
		}
	}

	UE_LOG(LogHazard, Log, TEXT("hh.Hazard.BreakWall — %d개 부쉈습니다."), BrokenCount);
}

static FAutoConsoleCommandWithWorld GHazardBreakWallCommand(
	  TEXT("hh.Hazard.BreakWall"),
	  TEXT("맵의 모든 BreakableWall 을 강제로 부순다 (점착 폭탄/브루트 연결 전 테스트용)"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&HazardBreakWallCommand),
	  ECVF_Cheat);

#endif   // !UE_BUILD_SHIPPING
