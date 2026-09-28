#include "Hazards/BreakableWall.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"        // TActorIterator — 디버그 치트(hh.Hazard.BreakWall)에서만 쓴다
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	DOREPLIFETIME(ABreakableWall, HitCount);
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

	++HitCount;

	UE_LOG(LogHazard, Log, TEXT("[BreakableWall:%s] 타격 %d / %d — 원인 %s"),
		*GetName(), HitCount, HitPoints, Breacher ? *Breacher->GetName() : TEXT("Unknown"));

	if (HitCount >= HitPoints)
	{
		bIsBroken = true;

		// 서버에서 직접 대입하면 RepNotify 가 안 불린다. 손으로 불러야 호스트 화면에서도 부서진다
		// (AVaultDoor::TryBreach 와 동일 사유)
		OnRep_bIsBroken();
		return true;
	}

	// 아직 안 부서졌다 — 균열만 진행한다. 서버 자신도 RepNotify 를 안 받으므로 손으로 부른다
	// (ULootDurabilityComponent::HandleLootImpact 와 동일 사유)
	OnRep_HitCount();
	return false;
}

void ABreakableWall::OnRep_bIsBroken()
{
	if (bIsBroken)
	{
		ApplyBreak();
	}
}

void ABreakableWall::OnRep_HitCount()
{
	// 이미 부서진 뒤라면 메시가 이미 숨겨졌다 — 균열 연출을 다시 씌울 이유가 없다.
	// (초기 복제 순서에 따라 OnRep_HitCount 가 OnRep_bIsBroken 보다 늦게 와도 안전하다)
	if (bIsBroken)
	{
		return;
	}

	ApplyCrackVisual();
	OnDamageAccumulated(HitCount, HitPoints);
}

float ABreakableWall::GetDamageRatio01() const
{
	// 한 방에 부서지는 벽은 '균열 단계' 자체가 없다 (ULootDurabilityComponent::GetDamageRatio01 과 동일 사유)
	if (HitPoints <= 1)
	{
		return 0.f;
	}

	// 분모가 HitPoints 보다 하나 적은 이유는 헤더의 GetDamageRatio01 주석 참고 —
	// 마지막 타격은 같은 프레임에 메시가 숨겨져 1.0 이 화면에 안 나오기 때문이다.
	const float Ratio = static_cast<float>(HitCount) / static_cast<float>(HitPoints - 1);
	return FMath::Clamp(Ratio, 0.f, 1.f);
}

void ABreakableWall::ApplyCrackVisual()
{
	if (CrackParameterName.IsNone() || !IsValid(WallMesh))
	{
		return;
	}

	const int32 SlotCount = WallMesh->GetNumMaterials();

	// 첫 타격에서만 만든다. 안 맞은 벽은 MID 를 하나도 들지 않는다
	// (ULootDurabilityComponent::ApplyCrackVisual 과 동일 사유)
	if (CrackMaterials.Num() != SlotCount)
	{
		CrackMaterials.Reset(SlotCount);

		bool bAnySlotHasParameter = false;
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			UMaterialInstanceDynamic* MID = WallMesh->CreateDynamicMaterialInstance(Slot);
			CrackMaterials.Add(MID);

			float Unused = 0.f;
			if (IsValid(MID) && MID->GetScalarParameterValue(FMaterialParameterInfo(CrackParameterName), Unused))
			{
				bAnySlotHasParameter = true;
			}
		}

		// 파라미터가 없으면 SetScalarParameterValue 는 조용히 아무 일도 안 한다.
		// 경고가 없으면 "머티리얼을 만들었는데 왜 안 갈라지지" 로 한참 헤맨다
		if (!bAnySlotHasParameter && !bWarnedMissingCrackParameter)
		{
			bWarnedMissingCrackParameter = true;
			UE_LOG(LogHazard, Warning,
				TEXT("[BreakableWall:%s] 머티리얼에 스칼라 파라미터 '%s' 가 없다. 균열 연출이 나오지 않는다 ")
				TEXT("(머티리얼에 파라미터를 추가하거나 CrackParameterName 을 None 으로 둘 것)"),
				*GetName(), *CrackParameterName.ToString());
		}
	}

	const float Ratio = GetDamageRatio01();
	for (UMaterialInstanceDynamic* MID : CrackMaterials)
	{
		if (IsValid(MID))
		{
			MID->SetScalarParameterValue(CrackParameterName, Ratio);
		}
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

static void HazardBreakWallCommand(const TArray<FString>& Args, UWorld* World)
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

	// 인자 없이 치면 예전처럼 완전히 부서질 때까지 반복한다(강제 파괴용).
	// 숫자를 주면 그만큼만 때린다 — 1이면 중간 균열 단계 하나만 확인하고 싶을 때 쓴다.
	// Safety 캡(10)은 HitPoints 를 비정상적으로 크게 잡아도 무한 루프에 안 빠지게 한다.
	const int32 RequestedHits = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 10;
	const int32 HitsToApply = FMath::Clamp(RequestedHits, 1, 10);

	int32 BrokenCount = 0;

	for (TActorIterator<ABreakableWall> It(World); It; ++It)
	{
		for (int32 i = 0; i < HitsToApply && !It->IsBroken(); ++i)
		{
			// ImpactRadius 0 — 브루트 돌진과 같은 "직접 타격" 취급으로 거리 판정 없이 때린다
			It->TryBreak(nullptr, It->GetActorLocation(), 0.f);
		}

		if (It->IsBroken())
		{
			++BrokenCount;
		}
	}

	UE_LOG(LogHazard, Log, TEXT("hh.Hazard.BreakWall %d — %d개 타격, %d개 부쉈습니다."),
		HitsToApply, HitsToApply, BrokenCount);
}

static FAutoConsoleCommandWithWorldAndArgs GHazardBreakWallCommand(
	  TEXT("hh.Hazard.BreakWall"),
	  TEXT("hh.Hazard.BreakWall [횟수] — 맵의 모든 BreakableWall 을 때린다. 인자 없으면 완전히 부술 때까지, 숫자를 주면 그만큼만(1=균열만 확인)"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HazardBreakWallCommand),
	  ECVF_Cheat);

#endif   // !UE_BUILD_SHIPPING
