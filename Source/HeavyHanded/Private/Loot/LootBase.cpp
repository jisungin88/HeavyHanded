#include "Loot/LootBase.h"

#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
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

    /** 놓을 때 운반자 몸 밖으로 밀어내며 추가로 띄우는 여유(cm) */
    constexpr float ReleaseDepenetrationMargin = 2.f;

    /** [임시] 조준점을 찾는 트레이스 길이(cm). 아무것도 안 맞으면 이 거리의 허공을 조준점으로 본다 */
    constexpr float DebugAimTraceDistance = 20000.f;

    /** [임시] 조준점이 발사점에서 이보다 가까우면 방향이 불안정해지므로 시선 방향으로 대체한다 */
    constexpr float DebugMinAimDistance = 150.f;
}

ALootBase::ALootBase()
{
    // 평소에는 매 프레임 할 일이 없다. 물리는 엔진이 돌린다.
    // 조준 중에만 궤적을 갱신해야 하므로, 틱 자체는 열어 두고 기본은 꺼 둔다.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

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

    Debug_SetupTestKeys();
}

void ALootBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 조준이 끝났거나 손에서 놓였으면 틱을 도로 끈다. 노획물이 수십 개 깔리므로
    // 필요 없는 틱을 계속 돌리지 않는다.
    APawn* Carrier = PrimaryCarrier.Get();
    if (!bDebugAiming || !IsValid(Carrier))
    {
        bDebugAiming = false;
        SetActorTickEnabled(false);
        return;
    }

    ShowThrowTrajectory(Debug_ComputeAimDirection());
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

    // 운반자의 이동 속도를 얼마나 섞을지는 데이터가 정한다. 기본은 0 이다.
    // 1:1 로 더하면 이동 속도가 ThrowSpeed 와 비슷할 때 조준이 무의미해진다.
    if (PhysicsData.CarrierVelocityInfluence > 0.f)
    {
        if (const APawn* Carrier = PrimaryCarrier.Get())
        {
            Velocity += Carrier->GetVelocity() * PhysicsData.CarrierVelocityInfluence;
        }
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
    APawn* PreviousCarrier = MoveIgnoredCarrier.Get();
    if (PreviousCarrier && PreviousCarrier != Carrier)
    {
        SetCarrierMoveIgnore(PreviousCarrier, false);
        MoveIgnoredCarrier = nullptr;
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

        // 물리를 켜기 전에 운반자와의 겹침을 푼다. 순서가 바뀌면 소용없다.
        // 위치 보정은 서버가 정하고 클라이언트는 복제로 받는다.
        if (HasAuthority())
        {
            ResolveReleaseOverlap(PreviousCarrier);
        }

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

void ALootBase::ResolveReleaseOverlap(const APawn* Carrier)
{
    if (!IsValid(Carrier))
    {
        return;
    }

    // ComputePenetration 이 비const 함수라 const 포인터로 받을 수 없다.
    UPrimitiveComponent* CarrierBody = Cast<UPrimitiveComponent>(Carrier->GetRootComponent());
    if (!CarrierBody)
    {
        return;
    }

    // 소지 중에는 물리가 꺼져 있어 노획물이 운반자 몸 안에 들어가 있어도 아무 일도 없다.
    // 그 상태로 물리를 켜면 물리 엔진이 겹침을 해소하느라 큰 임펄스를 만들고,
    // 그게 '세게 부딪혔다'로 잡혀 파손 카운트까지 올라간다. 놓기만 했는데 물건이 상한다.
    //
    // 상호 무시(IgnoreActorWhenMoving)로는 막을 수 없다. 그건 컴포넌트 이동 스윕에만
    // 적용되고, 시뮬레이션 중인 바디의 접촉은 물리 엔진이 따로 처리하기 때문이다.
    //
    // 그래서 물리를 켜기 전에 겹침을 직접 푼다. 최소 이동 거리(MTD)만큼만 밀어내므로
    // 실제로 겹쳐 있을 때만, 필요한 만큼만 움직인다.
    FMTDResult PenetrationResult;
    if (!CarrierBody->ComputePenetration(PenetrationResult,
        LootMesh->GetCollisionShape(), GetActorLocation(), GetActorQuat()))
    {
        // 겹치지 않았다. 손 소켓에 제대로 붙어 있으면 대개 이쪽이다.
        return;
    }

    // 딱 붙여 놓으면 부동소수 오차로 다시 겹친 것으로 잡힐 수 있어 여유를 조금 준다.
    const FVector SafeLocation = GetActorLocation()
        + PenetrationResult.Direction * (PenetrationResult.Distance + ReleaseDepenetrationMargin);

    SetActorLocation(SafeLocation, /*bSweep=*/false);
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

    // 게이팅 효과를 재려면 들어온 총량을 알아야 한다.
    ++DebugRawHitCount;

    // [1겹] 임계값 미만 무시.
    // 구르거나 미세하게 재접촉하는 것까지 전부 OnHit 으로 온다.
    if (ImpulseMagnitude < PhysicsData.ImpactReportThreshold)
    {
        ShowImpactDebug(
            FString::Printf(TEXT("기각(약함) %.0f < %.0f"),
                ImpulseMagnitude, PhysicsData.ImpactReportThreshold),
            FColor::Silver, Hit.ImpactPoint);
        return;
    }

    // [2겹] 같은 대상에 대한 짧은 시간 내 재발행 차단.
    // 임계값만으로는 못 막는다 — 세게 떨어지면 강한 충격이 연달아 여러 번 잡힌다.
    const float Now = World->GetTimeSeconds();
    if (!TryConsumeImpactCooldown(OtherActor, Now))
    {
        ShowImpactDebug(
            FString::Printf(TEXT("기각(%.1f초 내 재충돌) %.0f"), ImpactDebounceSeconds, ImpulseMagnitude),
            FColor::Orange, Hit.ImpactPoint);
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

    // 낙하 1회에 OnHit 5~15회가 확정 1회로 묶이는지를 이 비율로 확인한다.
    ++DebugConfirmedCount;
    ShowImpactDebug(
        FString::Printf(TEXT("확정 #%d  임펄스 %.0f  (OnHit 누적 %d회)"),
            DebugConfirmedCount, ImpulseMagnitude, DebugRawHitCount),
        FColor::Yellow, Event.ImpactPoint);

    // 예약된 원인은 1회성이다. 다음 충돌부터는 일반 충돌로 돌아간다.
    PendingImpactCause = ELootImpactCause::Collision;
    PendingInstigatorPawn = nullptr;
}

// --------------------------------------------------------------------------
// 디버그 — 플레이어 파트가 연결되면 Debug_ 함수들은 지운다
// --------------------------------------------------------------------------

void ALootBase::Debug_SetupTestKeys()
{
    // 판정이 서버 전용이라 클라이언트에 붙여 봐야 눌리지 않는다.
    if (!bDebugEnableTestKeys || !HasAuthority())
    {
        return;
    }

    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(this, 0);
    if (!IsValid(LocalPC))
    {
        return;
    }

    // 액터도 EnableInput 을 하면 InputComponent 를 받아 키를 직접 받을 수 있다.
    EnableInput(LocalPC);
    if (!InputComponent)
    {
        return;
    }

    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ALootBase::Debug_ToggleGrabByLocalPlayer);

    // 누르고 있는 동안 조준, 뗄 때 던진다.
    InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ALootBase::Debug_BeginThrowAim);
    InputComponent->BindKey(EKeys::T, IE_Released, this, &ALootBase::Debug_ThrowForward);
}

void ALootBase::Debug_ToggleGrabByLocalPlayer()
{
    APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsValid(LocalPawn))
    {
        // 에디터에서 (플레이 중이 아닐 때) 버튼이 눌린 경우.
        // 조용히 빠지면 원인을 못 찾으므로 로그라도 남긴다.
        UE_LOG(LogTemp, Warning, TEXT("[Loot:%s] 로컬 플레이어 폰이 없다. PIE 중인지 확인할 것."), *GetName());
        return;
    }

    // 들고 있으면 놓는다.
    if (PrimaryCarrier.Get() == LocalPawn)
    {
        OnReleased(LocalPawn);
        return;
    }

    // 남이 들고 있으면 건드리지 않는다.
    if (IsValid(PrimaryCarrier))
    {
        return;
    }

    // 키는 모든 노획물이 같이 받는다. 가까이 간 하나만 반응해야 한다.
    if (FVector::Dist(GetActorLocation(), LocalPawn->GetActorLocation()) > DebugGrabRange)
    {
        return;
    }

    OnGrabbed(LocalPawn);
}

FVector ALootBase::Debug_ComputeAimDirection() const
{
    const APawn* Carrier = PrimaryCarrier.Get();
    const UWorld* World = GetWorld();
    if (!IsValid(Carrier) || !World)
    {
        return FVector::ZeroVector;
    }

    // 카메라 시점. 컨트롤러가 있으면 실제 카메라를, 없으면 폰의 눈 위치를 쓴다.
    FVector ViewLocation;
    FRotator ViewRotation;
    if (const AController* CarrierController = Carrier->GetController())
    {
        CarrierController->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        Carrier->GetActorEyesViewPoint(ViewLocation, ViewRotation);
    }

    const FVector ViewDirection = ViewRotation.Vector();

    // 화면 중앙이 가리키는 지점을 찾는다.
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LootDebugAim), false, this);

    // 들고 있는 노획물은 카메라 바로 앞에 있어서 반드시 먼저 걸린다. 던진 사람도 뺀다.
    TraceParams.AddIgnoredActor(Carrier);

    FVector AimPoint = ViewLocation + ViewDirection * DebugAimTraceDistance;

    FHitResult AimHit;
    if (World->LineTraceSingleByChannel(AimHit, ViewLocation, AimPoint, ECC_Visibility, TraceParams))
    {
        AimPoint = AimHit.ImpactPoint;
    }

    // 벽에 바짝 붙으면 조준점이 발사점보다 뒤에 놓여 엉뚱한 방향이 나온다.
    // 그때는 시선 방향을 그대로 쓴다.
    const FVector ToAimPoint = AimPoint - GetActorLocation();
    if (ToAimPoint.SizeSquared() < FMath::Square(DebugMinAimDistance))
    {
        return ViewDirection;
    }

    return ToAimPoint.GetSafeNormal();
}

void ALootBase::Debug_BeginThrowAim()
{
    if (!IsValid(PrimaryCarrier))
    {
        return;
    }

    bDebugAiming = true;
    SetActorTickEnabled(true);
}

void ALootBase::Debug_ThrowForward()
{
    bDebugAiming = false;
    SetActorTickEnabled(false);

    APawn* Carrier = PrimaryCarrier.Get();
    if (!IsValid(Carrier))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::White,
                TEXT("먼저 G 로 잡아야 한다"));
        }
        return;
    }

    // 조준 중 궤적과 같은 함수를 쓴다. 두 곳에서 따로 구하면 보던 것과 다르게 날아간다.
    const FVector AimDirection = Debug_ComputeAimDirection();
    if (AimDirection.IsNearlyZero())
    {
        return;
    }

    // 던지기 전에 그려야 한다. OnThrown 이 PrimaryCarrier 를 비우면
    // 운반자 속도가 빠져서 예측과 실제가 달라진다.
    // 조준 중 궤적은 한 프레임짜리라 사라지므로, 비교용으로 6초짜리를 한 번 더 남긴다.
    ShowThrowTrajectory(AimDirection, 6.f);

    OnThrown(Carrier, AimDirection);
}

void ALootBase::ShowThrowTrajectory(const FVector& AimDirection, float Duration)
{
#if ENABLE_DRAW_DEBUG
    FPredictProjectilePathResult Result;
    if (!PredictThrowPath(AimDirection, Result))
    {
        // 아무데도 맞지 않아도 경로 자체는 그린다. 반환값은 충돌 여부일 뿐이다.
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (int32 Index = 1; Index < Result.PathData.Num(); ++Index)
    {
        DrawDebugLine(World,
            Result.PathData[Index - 1].Location, Result.PathData[Index].Location,
            FColor::Cyan, false, Duration, 0, 2.f);
    }

    // 예측한 착탄 지점. 실제로 여기 떨어지는지 보면 된다.
    if (Result.HitResult.bBlockingHit)
    {
        DrawDebugSphere(World, Result.HitResult.ImpactPoint, 20.f, 12, FColor::Cyan, false, Duration);
    }
#endif
}

void ALootBase::ShowImpactDebug(const FString& Message, const FColor& Color, const FVector& Location) const
{
    if (!bShowImpactDebug)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Loot:%s] %s"), *GetName(), *Message);

    if (GEngine)
    {
        // 키를 -1 로 주면 줄이 덮어써지지 않고 쌓인다. 몇 번 왔는지를 봐야 하므로 쌓아야 한다.
        GEngine->AddOnScreenDebugMessage(-1, 4.f, Color,
            FString::Printf(TEXT("[%s] %s"), *GetName(), *Message));
    }

#if ENABLE_DRAW_DEBUG
    // 어디에 부딪혔는지가 기각 사유를 읽는 데 필요하다 (바닥인지 벽인지 다른 물건인지).
    DrawDebugSphere(GetWorld(), Location, 12.f, 8, Color, false, 2.f);
#endif
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
