#include "Loot/LootBase.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"

namespace LootCollisionProfiles
{
    /** 물리 시뮬레이션 중 (Config/DefaultEngine.ini 의 CollisionProfile 정의) */
    static const FName Simulating(TEXT("Loot"));

    /** 소지 중 — 물리 OFF, 다른 캐릭터만 Block */
    static const FName Carried(TEXT("CarriedLoot"));
}

namespace
{
    /** 이 개수를 넘으면 만료된 디바운스 항목을 청소한다. 상시 순회를 피하기 위한 값 */
    constexpr int32 ImpactCooldownPruneThreshold = 8;
}

ALootBase::ALootBase()
{
    // 노획물 자체는 매 프레임 할 일이 없다. 물리는 엔진이 돌린다.
    PrimaryActorTick.bCanEverTick = false;

    bReplicates = true;
    SetReplicateMovement(true);

    // 물리 물체는 서버 스냅샷 간격이 곧 클라이언트가 보는 끊김이 된다.
    // NetServerMaxTickRate 를 60 으로 올려 둔 것과 짝이다. (Config/DefaultEngine.ini)
    NetUpdateFrequency = 60.f;
    MinNetUpdateFrequency = 20.f;

    LootMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootMesh"));
    SetRootComponent(LootMesh);
    LootMesh->SetCollisionProfileName(LootCollisionProfiles::Simulating);
    LootMesh->SetSimulatePhysics(true);

    // 이게 없으면 OnComponentHit 이 아예 오지 않는다. 물리 바디는 기본값이 꺼져 있다.
    LootMesh->SetNotifyRigidBodyCollision(true);
}

void ALootBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 등록을 빠뜨려도 컴파일 에러가 나지 않고 호스트에서는 멀쩡히 동작한다. 반드시 확인할 것.
    DOREPLIFETIME(ALootBase, PrimaryCarrier);
}

void ALootBase::BeginPlay()
{
    Super::BeginPlay();

    // 질량은 소음 크기와 던지기 충격량의 원천이므로 메시 기본값에 맡기지 않는다.
    LootMesh->SetMassOverrideInKg(NAME_None, PhysicsData.MassKg, true);

    // 클라이언트는 예측하지 않고 서버 스냅샷을 향해 속도 보간만 한다.
    // 클라이언트마다 물리 결과가 미세하게 달라서, 예측을 켜면 사람마다 다른 결과가 나온다.
    // (Config/DefaultEngine.ini 의 PhysicsSettings 주석과 짝을 이룬다)
    SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);

    // 서버에서만 판정하므로 클라이언트에는 델리게이트를 붙이지 않는다.
    if (HasAuthority())
    {
        LootMesh->OnComponentHit.AddDynamic(this, &ALootBase::HandleMeshHit);
    }

    // 스폰 직후 상태를 한 번 맞춘다 (레벨에 놓인 채 시작하는 경우 포함)
    ApplyCarryState();
}

// --------------------------------------------------------------------------
// ICarryable — 값 제공
// --------------------------------------------------------------------------

EWeightClass ALootBase::GetWeightClass() const
{
    return PhysicsData.WeightClass;
}

int32 ALootBase::GetRequiredCarriers() const
{
    return PhysicsData.RequiredCarriers;
}

float ALootBase::GetCarrySpeedMultiplier() const
{
    // 값만 준다. 실제 MaxWalkSpeed 조작은 플레이어 파트가 한다.
    // 양쪽에서 적용하면 배율이 두 번 곱해져 중량형이 기어간다.
    return PhysicsData.CarrySpeedMultiplier;
}

bool ALootBase::IsJumpAllowedWhileCarried() const
{
    return PhysicsData.bAllowJumpWhileCarried;
}

// --------------------------------------------------------------------------
// ICarryable — 판정 (서버 전용)
// --------------------------------------------------------------------------

bool ALootBase::CanBeCarriedBy(const APawn* Requester) const
{
    if (!IsValid(Requester))
    {
        return false;
    }

    // 이미 다른 사람이 들고 있으면 거부한다.
    if (IsValid(PrimaryCarrier) && PrimaryCarrier.Get() != Requester)
    {
        return false;
    }

    // 중량형의 '2인 필수'는 여기서 막지 않는다.
    // 리더가 먼저 잡고 팔로워가 합류하는 구조라서, 리더 한 명의 잡기까지 거부하면
    // 아무도 들 수 없다. 실제 인원 요건은 2인 협력 캐리 단계에서 팔로워 합류로 채운다.
    // 그때까지는 GetRequiredCarriers() 값만 제공한다.
    return true;
}

void ALootBase::OnGrabbed(APawn* Carrier)
{
    // Server RPC 는 요청일 뿐이다. 판정은 서버가 하고 클라이언트를 신뢰하지 않는다.
    if (!HasAuthority() || !CanBeCarriedBy(Carrier))
    {
        return;
    }

    PrimaryCarrier = Carrier;

    // 서버에서 직접 값을 바꾸면 OnRep 이 호출되지 않는다. 서버 몫은 손으로 부른다.
    ApplyCarryState();
}

void ALootBase::OnReleased(APawn* Carrier)
{
    if (!HasAuthority())
    {
        return;
    }

    // 들고 있지 않은 사람의 놓기 요청은 무시한다.
    if (PrimaryCarrier.Get() != Carrier)
    {
        return;
    }

    PrimaryCarrier = nullptr;

    // 놓은 직후 바닥에 닿는 첫 충격을 Drop 으로 표시한다.
    SetPendingImpactCause(ELootImpactCause::Drop, Carrier);

    ApplyCarryState();
}

bool ALootBase::CanBeThrown() const
{
    return PhysicsData.bAllowThrow;
}

void ALootBase::OnThrown(APawn* Carrier, const FVector& AimDirection)
{
    if (!HasAuthority())
    {
        return;
    }

    if (PrimaryCarrier.Get() != Carrier)
    {
        return;
    }

    // 던질 수 없는 물건은 요청을 씹지 않고 제자리에 놓는다.
    // 거부만 하면 플레이어는 입력이 먹지 않는 것으로 느낀다.
    if (!CanBeThrown())
    {
        OnReleased(Carrier);
        return;
    }

    // 운반자의 이동 속도를 더하기 때문에 PrimaryCarrier 를 비우기 전에 계산해야 한다.
    const FVector LaunchVelocity = ComputeThrowVelocity(AimDirection);

    PrimaryCarrier = nullptr;

    // 날아가서 처음 부딪히는 것이 던지기의 결과다. Drop 이 아니라 Throw 로 표시한다.
    SetPendingImpactCause(ELootImpactCause::Throw, Carrier);

    // 디태치 + 물리 ON + 프로파일 복구
    ApplyCarryState();

    // 손 소켓은 던진 사람 캡슐과 겹쳐 있다. 겹친 채로 물리를 켜면 물리 엔진이
    // 침투를 해소하느라 자기가 던진 물건에 튕겨 나간다. 먼저 간격을 만든다.
    const FVector LaunchDirection = AimDirection.GetSafeNormal();
    if (PhysicsData.ThrowClearance > 0.f && !LaunchDirection.IsNearlyZero())
    {
        SetActorLocation(GetActorLocation() + LaunchDirection * PhysicsData.ThrowClearance,
            /*bSweep=*/false);
    }

    // 임펄스 = 질량 x 목표 속도.
    // 질량을 곱해야 무게와 무관하게 데이터에 적은 ThrowSpeed 그대로 나간다.
    // 무거운 물건이 덜 날아가는 것은 ThrowSpeed 값으로 표현한다. (값은 데이터, 행동은 공통)
    LootMesh->AddImpulse(LaunchVelocity * LootMesh->GetMass());

    // 회전이 없으면 물건이 미끄러지듯 날아가 던진 느낌이 안 난다.
    // 조준 방향 기준 오른쪽 축으로 굴린다. 위/아래로 똑바로 던지면 축이 0 이라 회전은 생략된다.
    if (PhysicsData.ThrowSpinSpeed > 0.f)
    {
        const FVector SpinAxis =
            FVector::CrossProduct(LaunchDirection, FVector::UpVector).GetSafeNormal();
        if (!SpinAxis.IsNearlyZero())
        {
            LootMesh->SetPhysicsAngularVelocityInDegrees(SpinAxis * PhysicsData.ThrowSpinSpeed);
        }
    }
}

FVector ALootBase::ComputeThrowVelocity(const FVector& AimDirection) const
{
    const FVector Aim = AimDirection.GetSafeNormal();
    if (Aim.IsNearlyZero())
    {
        return FVector::ZeroVector;
    }

    // 조준 방향에 위쪽 성분을 섞어 포물선을 만든다.
    const FVector LaunchDirection =
        (Aim + FVector::UpVector * PhysicsData.ThrowUpwardRatio).GetSafeNormal();

    FVector Velocity = LaunchDirection * PhysicsData.ThrowSpeed;

    // 달리면서 던지면 그만큼 더 나간다. 물리 운반 게임에서 이게 있고 없고가 체감 차이가 크다.
    if (const APawn* Carrier = PrimaryCarrier.Get())
    {
        Velocity += Carrier->GetVelocity();
    }

    return Velocity;
}

bool ALootBase::PredictThrowPath(const FVector& AimDirection, FPredictProjectilePathResult& OutResult)
{
    const FVector LaunchDirection = AimDirection.GetSafeNormal();
    if (LaunchDirection.IsNearlyZero())
    {
        return false;
    }

    FPredictProjectilePathParams Params;

    // 실제 던지기와 같은 출발점·속도를 써야 미리 보이는 궤적이 맞는다.
    Params.StartLocation = GetActorLocation() + LaunchDirection * PhysicsData.ThrowClearance;
    Params.LaunchVelocity = ComputeThrowVelocity(AimDirection);
    Params.ProjectileRadius = LootMesh->Bounds.SphereRadius;

    Params.bTraceWithCollision = true;
    Params.bTraceWithChannel = true;
    Params.TraceChannel = ECollisionChannel::ECC_WorldStatic;

    // 자기 자신과 던지는 사람은 궤적에서 빼야 조준선이 발밑에서 끊기지 않는다.
    Params.ActorsToIgnore.Add(this);
    if (APawn* Carrier = PrimaryCarrier.Get())
    {
        Params.ActorsToIgnore.Add(Carrier);
    }

    Params.MaxSimTime = 3.f;
    Params.SimFrequency = 15.f;

    return UGameplayStatics::PredictProjectilePath(this, Params, OutResult);
}

APawn* ALootBase::GetPrimaryCarrier() const
{
    return PrimaryCarrier;
}

UPrimitiveComponent* ALootBase::GetPhysicsRoot() const
{
    return LootMesh;
}

// --------------------------------------------------------------------------
// 소지 상태
// --------------------------------------------------------------------------

void ALootBase::OnRep_PrimaryCarrier()
{
    ApplyCarryState();
}

void ALootBase::ApplyCarryState()
{
    APawn* Carrier = PrimaryCarrier;
    const bool bCarried = IsValid(Carrier);

    // 운반자가 바뀌거나 놓인 경우, 이전 운반자와의 상호 무시를 먼저 푼다.
    if (APawn* Previous = MoveIgnoredCarrier.Get())
    {
        if (Previous != Carrier)
        {
            SetCarrierMoveIgnore(Previous, false);
            MoveIgnoredCarrier = nullptr;
        }
    }

    if (bCarried)
    {
        // PhysicsHandle 로 물리를 유지한 채 드는 방식은 멀티에서 깨진다.
        // 물리를 끄고 Attach 한 뒤, 놓기/던지기 순간에만 다시 켠다.
        LootMesh->SetSimulatePhysics(false);
        LootMesh->SetCollisionProfileName(LootCollisionProfiles::Carried);

        // 소지 중 프로파일은 다른 캐릭터를 Block 한다. 그대로 두면 운반자 본인도 막혀서
        // 자기 물건에 걸려 움직이지 못한다. 이동 스윕은 각 머신에서 로컬로 돌기 때문에
        // 서버·클라이언트 양쪽에서 무시를 걸어야 한다. (그래서 이 함수가 ApplyCarryState 안에 있다)
        SetCarrierMoveIgnore(Carrier, true);
        MoveIgnoredCarrier = Carrier;

        AttachToCarrier(Carrier);
    }
    else
    {
        // 어태치된 채로는 물리가 돌지 않는다. 반드시 먼저 떼어낸다.
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

        LootMesh->SetCollisionProfileName(LootCollisionProfiles::Simulating);
        LootMesh->SetSimulatePhysics(true);
    }
}

void ALootBase::AttachToCarrier(APawn* Carrier)
{
    // 손 소켓은 스켈레탈 메시에 있다. 캐릭터가 아니거나 메시가 없으면 루트에 붙여 최소한 따라다니게 한다.
    USceneComponent* AttachTarget = Carrier->GetRootComponent();
    FName SocketToUse = NAME_None;

    if (const ACharacter* CarrierCharacter = Cast<ACharacter>(Carrier))
    {
        if (USkeletalMeshComponent* CarrierMesh = CarrierCharacter->GetMesh())
        {
            AttachTarget = CarrierMesh;

            // 소켓이 없는데 이름을 넘기면 메시 원점에 조용히 붙어 원인을 찾기 어렵다.
            // 존재를 확인하고 넘긴다.
            if (CarrierMesh->DoesSocketExist(CarrySocketName))
            {
                SocketToUse = CarrySocketName;
            }
        }
    }

    if (!AttachTarget)
    {
        return;
    }

    // 노획물마다 크기가 다르므로 스케일은 자기 것을 유지한다.
    AttachToComponent(AttachTarget,
        FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketToUse);
}

void ALootBase::SetCarrierMoveIgnore(APawn* Carrier, bool bIgnore)
{
    if (!IsValid(Carrier))
    {
        return;
    }

    if (UPrimitiveComponent* CarrierRoot = Cast<UPrimitiveComponent>(Carrier->GetRootComponent()))
    {
        CarrierRoot->IgnoreActorWhenMoving(this, bIgnore);
    }

    LootMesh->IgnoreActorWhenMoving(Carrier, bIgnore);
}

// --------------------------------------------------------------------------
// 충돌 게이팅
// --------------------------------------------------------------------------

void ALootBase::SetPendingImpactCause(ELootImpactCause InCause, APawn* InInstigator)
{
    if (!HasAuthority())
    {
        return;
    }

    PendingImpactCause = InCause;
    PendingInstigatorPawn = InInstigator;
}

void ALootBase::ReportImpact(ELootImpactCause InCause, float ImpulseMagnitude,
    const FVector& ImpactPoint, APawn* InInstigator)
{
    if (!HasAuthority())
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FLootImpactEvent Event;
    Event.ImpactPoint = ImpactPoint;
    Event.ImpulseMagnitude = ImpulseMagnitude;
    Event.Cause = InCause;
    Event.LootActor = this;
    Event.InstigatorPawn = InInstigator;
    Event.ServerTime = World->GetTimeSeconds();

    // 부딪힌 표면이 없는 사건이므로 노멀은 위쪽, 재질은 기본값으로 둔다.
    // 파괴음의 재질(유리/나무)은 부딪힌 바닥이 아니라 노획물 자신의 것이므로,
    // 소음 파트가 Cause == Break 일 때 LootActor 에서 직접 읽는다.
    Event.ImpactNormal = FVector::UpVector;

    OnLootImpact.Broadcast(Event);
}

void ALootBase::HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 클라이언트의 물리 충돌은 보고받지 않는다. 시뮬레이션 결과가 머신마다 다르다.
    if (!HasAuthority())
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float ImpulseMagnitude = NormalImpulse.Size();

    // [1겹] 임계값 미만 무시.
    // 구르거나 미세하게 재접촉하는 것까지 전부 OnHit 으로 온다.
    if (ImpulseMagnitude < PhysicsData.ImpactReportThreshold)
    {
        return;
    }

    // [2겹] 같은 대상에 대한 짧은 시간 내 재발행 차단.
    // 임계값만으로는 못 막는다 — 세게 떨어지면 강한 충격이 연달아 여러 번 잡힌다.
    const float Now = World->GetTimeSeconds();
    if (!TryConsumeImpactCooldown(OtherActor, Now))
    {
        return;
    }

    FLootImpactEvent Event;
    Event.ImpactPoint = Hit.ImpactPoint;
    Event.ImpactNormal = Hit.ImpactNormal;
    Event.ImpulseMagnitude = ImpulseMagnitude;
    Event.Cause = PendingImpactCause;
    Event.LootActor = this;
    Event.InstigatorPawn = PendingInstigatorPawn;
    Event.ServerTime = Now;

    // 물리 충돌 콜백의 FHitResult 는 PhysMaterial 이 비어 오는 경우가 있어
    // 부딪힌 컴포넌트의 바디에서 직접 한 번 더 찾는다. (재질별 소음 차이의 근거 값)
    const UPhysicalMaterial* SurfaceMaterial = Hit.PhysMaterial.Get();
    if (!SurfaceMaterial && OtherComp)
    {
        if (const FBodyInstance* OtherBody = OtherComp->GetBodyInstance())
        {
            SurfaceMaterial = OtherBody->GetSimplePhysicalMaterial();
        }
    }
    if (SurfaceMaterial)
    {
        Event.SurfaceType = SurfaceMaterial->SurfaceType;
    }

    // 여기까지 온 것이 '확정 충격 1개'다.
    // 소음 파트와 파손 컴포넌트가 이 하나를 같이 소비한다.
    // 아이템은 물리적 사실만 알린다 — 얼마나 시끄러운지는 판단하지 않는다.
    OnLootImpact.Broadcast(Event);

    // 예약된 원인은 1회성이다. 다음 충돌부터는 일반 충돌로 돌아간다.
    PendingImpactCause = ELootImpactCause::Collision;
    PendingInstigatorPawn = nullptr;
}

bool ALootBase::TryConsumeImpactCooldown(const AActor* OtherActor, float Now)
{
    const TWeakObjectPtr<const AActor> Key(OtherActor);

    if (const float* LastTime = RecentImpactTimes.Find(Key))
    {
        if (Now - *LastTime < ImpactDebounceSeconds)
        {
            return false;
        }
    }

    RecentImpactTimes.Add(Key, Now);

    // 여러 대상에 계속 부딪히면 맵이 무한히 커진다. 만료·소멸된 항목을 걷어낸다.
    // 방금 넣은 항목은 경과 시간이 0 이라 여기서 지워지지 않는다.
    if (RecentImpactTimes.Num() > ImpactCooldownPruneThreshold)
    {
        for (auto It = RecentImpactTimes.CreateIterator(); It; ++It)
        {
            if (It.Key().IsStale() || Now - It.Value() >= ImpactDebounceSeconds)
            {
                It.RemoveCurrent();
            }
        }
    }

    return true;
}
