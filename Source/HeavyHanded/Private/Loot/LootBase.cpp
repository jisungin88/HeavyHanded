#include "Loot/LootBase.h"

#include "Components/InputComponent.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Loot/LootLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"                 // TActorIterator — 디버그 집기 대상 탐색
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"

// 노획물 액터와 하위 컴포넌트가 같은 카테고리로 찍는다. 선언은 Loot/LootLog.h.
DEFINE_LOG_CATEGORY(LogLoot);

namespace LootCollisionProfiles
{
    /** 물리 시뮬레이션 중 (Config/DefaultEngine.ini 의 CollisionProfile 정의) */
    static const FName Simulating(TEXT("Loot"));

    /** 소지 중 — 물리 OFF, 다른 캐릭터만 Block */
    static const FName Carried(TEXT("CarriedLoot"));
}

/**
 * 물리 복제 모드 폴백. 0 = PredictiveInterpolation(기본) / 1 = Default / 2 = Resimulation.
 *
 * PredictiveInterpolation 은 UE 5.4 에서 (WIP) 로 표시된 기능이다. 물건이 사람마다
 * 다른 곳에 있거나 심하게 튀는 증상이 나오면 이 값으로 엔진 기본 모드로 되돌려
 * '우리 코드 문제인지 엔진 모드 문제인지' 를 먼저 가른다.
 *
 * BeginPlay 에서만 읽는다. 이미 스폰된 노획물에는 적용되지 않으므로 PIE 를 다시 시작할 것.
 * 전역 스위치인 이유는 진단할 때 전부 한꺼번에 바꿔야 비교가 되기 때문이다
 * (액터별 스위치는 bShowImpactDebug 처럼 '하나만 보고 싶을 때' 쓴다 — 문서 07 테스트).
 */
static TAutoConsoleVariable<int32> CVarLootPhysicsRepMode(
    TEXT("hh.Loot.PhysicsRepMode"),
    0,
    TEXT("노획물 물리 복제 모드. 0=PredictiveInterpolation(기본) 1=Default 2=Resimulation. BeginPlay 에서만 읽는다"),
    ECVF_Default);

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

    /**
     * [임시] 디버그 키를 한 프레임에 한 번만 처리하기 위한 표식.
     *
     * 노획물은 각자 자기 InputComponent 로 키를 받는다. 즉 레벨에 노획물이 N 개면
     * G 를 한 번 눌러도 핸들러가 N 번 불린다. 대상 선정은 어차피 월드 전체를 훑어
     * 같은 답을 내므로, 첫 호출만 처리하고 나머지는 여기서 버린다.
     * (토글인 G 를 N 번 처리하면 잡았다 놨다를 반복해 결과가 뒤집힌다)
     */
    bool Debug_ClaimKeyPress(uint64& LastHandledFrame)
    {
        if (LastHandledFrame == GFrameCounter)
        {
            return false;
        }

        LastHandledFrame = GFrameCounter;
        return true;
    }
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
    DOREPLIFETIME(ALootBase, CurrentValue);
    DOREPLIFETIME(ALootBase, LootTypeTags);
    DOREPLIFETIME(ALootBase, LootStateTag);
}

void ALootBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
    TagContainer.AppendTags(LootTypeTags);

    if (LootStateTag.IsValid())
    {
        TagContainer.AddTag(LootStateTag);
    }
}

void ALootBase::AddLootTypeTag(const FGameplayTag& TypeTag)
{
    // 특성은 스폰 시점에 확정되고 그 뒤로 바뀌지 않는다. 서버가 정하고 클라이언트는 받는다.
    if (!HasAuthority() || !TypeTag.IsValid())
    {
        return;
    }

    LootTypeTags.AddTag(TypeTag);
}

void ALootBase::SetLootStateTag(const FGameplayTag& NewState)
{
    if (!HasAuthority() || LootStateTag == NewState)
    {
        return;
    }

    LootStateTag = NewState;
}

void ALootBase::BeginPlay()
{
    Super::BeginPlay();

    // 질량은 소음 크기와 던지기 충격량의 원천이므로 메시 기본값에 맡기지 않는다.
    LootMesh->SetMassOverrideInKg(NAME_None, PhysicsData.MassKg, true);

    // 가치는 서버가 정하고 클라이언트는 복제로 받는다.
    // 생성자가 아니라 여기서 넣어야 BP 가 지정한 BaseValue 가 반영된다.
    if (HasAuthority())
    {
        CurrentValue = BaseValue;
        OnRep_CurrentValue();

        // 중량형은 전용 컴포넌트가 없어서 여기서 직접 단다.
        // 파손형·불안정형은 각자 컴포넌트가 BeginPlay 에서 자기 태그를 등록한다.
        if (PhysicsData.WeightClass == EWeightClass::Heavy)
        {
            AddLootTypeTag(HHTags::Loot_Type_Heavy);
        }

        // 특성 태그가 하나도 없으면 플레이어 파트의 집기 트레이스가 이 액터를 그냥 지나친다.
        // Loot.Type 부모 태그 하나로 판정하므로, 특성이 없는 평범한 노획물도 부모는 달아야 한다.
        if (LootTypeTags.IsEmpty())
        {
            AddLootTypeTag(HHTags::Loot_Type);
        }

        SetLootStateTag(HHTags::Loot_State_Idle);
    }

    // 클라이언트는 예측하지 않고 서버 스냅샷을 향해 속도 보간만 한다.
    // 클라이언트마다 물리 결과가 미세하게 달라서, 예측을 켜면 사람마다 다른 결과가 나온다.
    // (Config/DefaultEngine.ini 의 PhysicsSettings 주석과 짝을 이룬다)
    //
    // UE 5.4 에는 전역 ini 키가 없어서 새 물리 액터마다 여기서 직접 불러야 한다.
    // (WIP) 기능이라 hh.Loot.PhysicsRepMode 로 엔진 기본 모드로 되돌릴 수 있게 열어 둔다.
    EPhysicsReplicationMode RepMode = EPhysicsReplicationMode::PredictiveInterpolation;
    switch (CVarLootPhysicsRepMode.GetValueOnGameThread())
    {
    case 1:  RepMode = EPhysicsReplicationMode::Default;      break;
    case 2:  RepMode = EPhysicsReplicationMode::Resimulation; break;
    default: break;
    }

    if (RepMode != EPhysicsReplicationMode::PredictiveInterpolation)
    {
        // 기본값에서 벗어난 상태로 테스트하다 원인을 착각하는 일이 없도록 남긴다.
        UE_LOG(LogLoot, Warning,
            TEXT("[Loot:%s] 물리 복제 모드가 기본(PredictiveInterpolation)이 아니다 — hh.Loot.PhysicsRepMode=%d"),
            *GetName(), CVarLootPhysicsRepMode.GetValueOnGameThread());
    }

    SetPhysicsReplicationMode(RepMode);

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

    // 손에서 놓였으면 틱을 도로 끈다. 노획물이 수십 개 깔리므로
    // 필요 없는 틱을 계속 돌리지 않는다.
    APawn* Carrier = PrimaryCarrier.Get();
    if (!IsValid(Carrier))
    {
        bDebugAiming = false;
        bCarriedWithoutSocket = false;
        SetActorTickEnabled(false);
        return;
    }

    if (bCarriedWithoutSocket && HasAuthority())
    {
        UpdateNoSocketCarryTransform(Carrier);
    }

    if (bDebugAiming)
    {
        ShowThrowTrajectory(ComputeThrowAimDirection());
    }
    else if (!bCarriedWithoutSocket)
    {
        SetActorTickEnabled(false);
    }
}

void ALootBase::UpdateNoSocketCarryTransform(const APawn* Carrier)
{
    // 폰의 액터 회전은 카메라를 따라간다는 보장이 없다. 액터 정면을 쓰면
    // 고개만 돌렸을 때 물건이 제자리에 남아 시야에서 사라진다. 시선을 기준으로 잡는다.
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

    // 시야 앞의 이 지점에 '손'이 있다고 본다.
    const FVector GripPoint = ViewLocation + ViewRotation.RotateVector(NoSocketCarryOffset);

    // 잡은 지점이 그 자리에 고정되도록 액터 원점을 반대로 밀어 준다.
    // 기울기가 바뀌면 원점도 손을 축으로 같이 돌아서, 아랫부분은 손에 붙어 있고
    // 윗부분만 넘어가는 모습이 된다. (기울기 자체는 불안정형 컴포넌트가 넣는다)
    //
    // 어태치된 상태라 월드 위치를 넣으면 엔진이 상대 위치를 계산해 준다.
    // 회전은 건드리지 않는다 — 불안정형의 관성 기울기가 상대 회전으로 들어가 있다.
    SetActorLocation(GripPoint - GetActorQuat().RotateVector(GetCarryGripOffset()),
        /*bSweep=*/false);
}

FVector ALootBase::GetCarryGripOffset() const
{
    if (!bCarryGripAtBottom || !IsValid(LootMesh))
    {
        return FVector::ZeroVector;
    }

    // 메시 로컬 기준 바운드. 원점이 메시 중앙에 있다는 보장이 없으므로 실제 아랫면을 찾는다.
    const FBoxSphereBounds LocalBounds = LootMesh->CalcLocalBounds();

    // 아랫면 중심. 로컬 값이라 액터 스케일을 곱해 월드 거리로 바꿔 둔다.
    return FVector(LocalBounds.Origin.X, LocalBounds.Origin.Y,
        LocalBounds.Origin.Z - LocalBounds.BoxExtent.Z) * LootMesh->GetComponentScale();
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

// --------------------------------------------------------------------------
// 가치
// --------------------------------------------------------------------------

void ALootBase::ApplyValueLoss(float LossRatio)
{
    // 가치는 정산에 직결되므로 서버만 정한다. 클라이언트 계산을 신뢰하지 않는다.
    if (!HasAuthority())
    {
        return;
    }

    const int32 PreviousValue = CurrentValue;

    // 이미 깎인 가치를 기준으로 다시 깎는다. 유출이 두 번 나면 두 번 줄어야 한다.
    CurrentValue = FMath::Clamp(
        FMath::RoundToInt(CurrentValue * (1.f - FMath::Clamp(LossRatio, 0.f, 1.f))),
        0, CurrentValue);

    if (CurrentValue == PreviousValue)
    {
        return;
    }

    // 서버에서 값을 직접 바꾸면 RepNotify 가 불리지 않는다. 서버 몫은 손으로 부른다.
    OnRep_CurrentValue();

    ShowImpactDebug(
        FString::Printf(TEXT("가치 %d -> %d ($%d 손실)"),
            PreviousValue, CurrentValue, PreviousValue - CurrentValue),
        FColor::Green, GetActorLocation());
}

void ALootBase::OnRep_CurrentValue()
{
    OnValueChanged(CurrentValue, BaseValue);
}

void ALootBase::ApplyCarryState()
{
    APawn* Carrier = PrimaryCarrier;
    const bool bCarried = IsValid(Carrier);

    // 이미 같은 상태를 반영해 뒀으면 아무것도 하지 않는다.
    //
    // 이 함수는 여러 경로에서 불린다 — 서버의 OnGrabbed/OnReleased/OnThrown,
    // 클라이언트의 OnRep_PrimaryCarrier, 그리고 앞으로 플레이어 파트가
    // ICarryable 로 알려 주는 시점. 캐릭터와 노획물은 서로 다른 액터 채널이라
    // 도착 순서가 보장되지 않고, 같은 상태가 연달아 들어오는 일이 실제로 생긴다.
    //
    // 그때 놓기 경로를 다시 타면 ApplyDropImpulse 가 한 번 더 들어가 놓은 물건이
    // 혼자 튀어 나가고, 잡기 경로를 다시 타면 어태치가 두 번 걸린다.
    // 상태를 반영하는 함수는 몇 번 불러도 결과가 같아야 한다.
    //
    // bCarryStateApplied 가 따로 필요한 이유: BeginPlay 의 첫 호출은 Carrier 가
    // 없는 상태(둘 다 nullptr)라 이 검사만으로는 초기화 자체를 건너뛰게 된다.
    if (bCarryStateApplied && AppliedCarrier.Get() == Carrier)
    {
        return;
    }

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

        SetLootStateTag(HHTags::Loot_State_Carried);
    }
    else
    {
        bCarriedWithoutSocket = false;

        // 들려 있던 것이 놓였을 때만 Dropped 로 간다.
        //
        // BeginPlay 에서도 이 함수가 한 번 도는데, 그때 무조건 덮어쓰면 레벨에 배치된
        // 노획물이 시작하자마자 "사람이 내려놓은 것"이 된다. 둘은 구별돼야 한다 —
        // 파괴(Broken)처럼 되돌아가지 않는 상태를 지우는 문제도 같이 막힌다.
        if (LootStateTag == HHTags::Loot_State_Carried)
        {
            SetLootStateTag(HHTags::Loot_State_Dropped);
        }

        // 어태치된 채로는 물리가 돌지 않는다. 반드시 먼저 떼어낸다.
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

        LootMesh->SetCollisionProfileName(LootCollisionProfiles::Simulating);

        // 물리를 켜기 전에 몸 밖으로 빼낸다. 순서가 바뀌면 소용없다.
        // 위치 보정은 서버가 정하고 클라이언트는 복제로 받는다.
        if (HasAuthority())
        {
            ResolveReleaseOverlap(PreviousCarrier);
        }

        LootMesh->SetSimulatePhysics(true);

        // 임펄스는 물리를 켠 뒤에야 먹는다.
        // 던지기는 OnThrown 이 직접 훨씬 큰 임펄스를 주므로 여기서는 건드리지 않는다.
        if (HasAuthority() && PendingImpactCause != ELootImpactCause::Throw)
        {
            ApplyDropImpulse(PreviousCarrier);
        }
    }

    // 무엇을 반영했는지 남긴다. 위쪽 조기 반환이 이 값을 본다.
    bCarryStateApplied = true;
    AppliedCarrier = Carrier;
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

    // 소켓이 있으면 소켓 위치가 곧 정답이고 손을 따라 알아서 움직인다.
    // 소켓이 없으면 폰 원점(= 카메라 자리)에 붙어서 화면을 가리거나 아예 안 보이고,
    // 고개를 돌려도 따라오지 않는다. 그때만 매 프레임 시선 앞에 다시 놓는다.
    bCarriedWithoutSocket = SocketToUse.IsNone();
    if (bCarriedWithoutSocket)
    {
        SetActorTickEnabled(true);
        UpdateNoSocketCarryTransform(Carrier);
    }
}

void ALootBase::ResolveReleaseOverlap(const APawn* Carrier)
{
    if (!IsValid(Carrier))
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 던지기는 손 위치에서 출발해야 한다. OnThrown 이 ApplyCarryState 를 부르기 직전에
    // 원인을 Throw 로 예약해 두므로 그것으로 구분한다. 여기서 위치를 옮기면
    // 손 높이 기준으로 그린 예측 궤적과 실제 궤적이 어긋난다.
    // (던지기는 OnThrown 이 ThrowClearance 로 따로 간격을 만든다)
    if (PendingImpactCause == ELootImpactCause::Throw)
    {
        return;
    }

    const FCollisionShape LootShape = LootMesh->GetCollisionShape();

    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LootRelease), /*bTraceComplex=*/false, this);
    TraceParams.AddIgnoredActor(Carrier);

    // --- 1) 운반자 몸 밖으로 빼낸다 ---------------------------------------
    //
    // [핵심] 소지 중 노획물은 운반자 몸 '안'에 들어가 있다. 그 상태로 물리를 켜면
    // 물리 엔진이 침투를 밀어내며 큰 임펄스를 만들고, 그게 '세게 부딪혔다'로 잡혀
    // 놓기만 했는데 파손 1회가 쌓인다. 바닥에 떨어져서 나는 파손과는 별개의 사건이다.
    //
    // 상호 무시(IgnoreActorWhenMoving)로는 못 막는다. 그건 컴포넌트 이동 스윕에만
    // 적용되고, 시뮬레이션 중인 바디의 접촉은 물리 엔진이 따로 처리하기 때문이다.
    //
    // MTD(ComputePenetration) 단독으로도 못 막는다. 두 형상의 중심이 거의 겹치면
    // 밀어낼 방향이 정해지지 않아 0 벡터가 나오고, 결국 제자리에 그대로 남는다.
    // 손 소켓이 없는 폰에 붙으면 노획물이 폰 원점에 정확히 겹치므로 딱 이 경우가 된다.
    //
    // 그래서 방향을 우리가 정한다. 운반자 정면으로, 두 형상의 반경 합만큼 빼낸다.
    // 버리기가 앞으로 던지는 동작이므로 방향도 이쪽이 맞다.
    FVector ForwardDir = Carrier->GetActorForwardVector();
    ForwardDir.Z = 0.f;
    ForwardDir = ForwardDir.GetSafeNormal();
    if (ForwardDir.IsNearlyZero())
    {
        ForwardDir = FVector::ForwardVector;
    }

    const float ClearDistance = Carrier->GetSimpleCollisionRadius()
        + LootMesh->Bounds.SphereRadius + ReleaseForwardClearance;

    const FVector StartLocation = GetActorLocation();
    FVector PlaceLocation = FVector(Carrier->GetActorLocation().X, Carrier->GetActorLocation().Y, StartLocation.Z)
        + ForwardDir * ClearDistance;

    // 앞이 벽이면 벽을 뚫고 놓게 된다. 실제로 갈 수 있는 데까지만 간다.
    // (운반자는 무시 대상이라 운반자 몸에는 걸리지 않는다)
    FHitResult ClearHit;
    if (World->SweepSingleByProfile(ClearHit, StartLocation, PlaceLocation, GetActorQuat(),
        LootCollisionProfiles::Simulating, LootShape, TraceParams))
    {
        PlaceLocation = ClearHit.Location;
    }

    SetActorLocation(PlaceLocation, /*bSweep=*/false);

    // --- 2) 그래도 겹쳐 있으면 마지막으로 밀어낸다 -------------------------
    //
    // 좁은 구석처럼 앞으로 뺄 공간이 없어 운반자 몸 안에 머무는 경우가 남는다.
    // ComputePenetration 이 비const 함수라 const 포인터로 받을 수 없다.
    if (UPrimitiveComponent* CarrierBody = Cast<UPrimitiveComponent>(Carrier->GetRootComponent()))
    {
        FMTDResult PenetrationResult;
        if (CarrierBody->ComputePenetration(PenetrationResult, LootShape,
            PlaceLocation, GetActorQuat()))
        {
            // 중심이 겹쳐 방향이 0 으로 나오면 MTD 를 믿을 수 없다. 정면으로 밀어낸다.
            const FVector PushDir = PenetrationResult.Direction.IsNearlyZero()
                ? ForwardDir : PenetrationResult.Direction;

            PlaceLocation += PushDir * (PenetrationResult.Distance + ReleaseDepenetrationMargin);
            SetActorLocation(PlaceLocation, /*bSweep=*/false);

            ShowImpactDebug(TEXT("버리기 — MTD 로 추가 이탈"), FColor::Magenta, PlaceLocation);
        }
    }

    // 이 값이 0 에 가까우면 몸 밖으로 못 빠져나간 것이다.
    ShowImpactDebug(
        FString::Printf(TEXT("버리기 — 몸 밖으로 %.0fcm"), FVector::Dist(StartLocation, PlaceLocation)),
        FColor::Cyan, PlaceLocation);
}

void ALootBase::ApplyDropImpulse(const APawn* Carrier)
{
    if (!IsValid(Carrier) || PhysicsData.DropSpeed <= 0.f)
    {
        return;
    }

    // 보는 방향으로 버린다. 액터 정면이 아니라 시선을 쓰는 이유는,
    // 발밑을 보고 버리면 발밑에 놓이고 앞을 보고 버리면 앞으로 가야 자연스럽기 때문이다.
    FVector DropDirection = Carrier->GetBaseAimRotation().Vector();
    if (DropDirection.IsNearlyZero())
    {
        DropDirection = Carrier->GetActorForwardVector();
    }

    // 위쪽 성분을 섞어 살짝 떠서 굴러가게 한다. 안 섞으면 바닥으로 미끄러지듯 밀린다.
    DropDirection = (DropDirection.GetSafeNormal()
        + FVector::UpVector * PhysicsData.DropUpwardRatio).GetSafeNormal();

    // 임펄스 = 질량 x 목표 속도. 질량을 곱해야 무게와 무관하게 DropSpeed 그대로 나간다.
    // 던지기와 계산은 같고 값만 훨씬 작다. 버리기는 조준도 궤적 예측도 없다.
    LootMesh->AddImpulse(DropDirection * PhysicsData.DropSpeed * LootMesh->GetMass());

    if (PhysicsData.DropSpinSpeed > 0.f)
    {
        const FVector SpinAxis =
            FVector::CrossProduct(DropDirection, FVector::UpVector).GetSafeNormal();
        if (!SpinAxis.IsNearlyZero())
        {
            LootMesh->SetPhysicsAngularVelocityInDegrees(SpinAxis * PhysicsData.DropSpinSpeed);
        }
    }
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
            FColor::Silver, Hit.ImpactPoint, ImpulseMagnitude);
        return;
    }

    // [2겹] 같은 대상에 대한 짧은 시간 내 재발행 차단.
    // 임계값만으로는 못 막는다 — 세게 떨어지면 강한 충격이 연달아 여러 번 잡힌다.
    const float Now = World->GetTimeSeconds();
    if (!TryConsumeImpactCooldown(OtherActor, Now))
    {
        ShowImpactDebug(
            FString::Printf(TEXT("기각(%.1f초 내 재충돌) %.0f"), ImpactDebounceSeconds, ImpulseMagnitude),
            FColor::Orange, Hit.ImpactPoint, ImpulseMagnitude);
        return;
    }

    FLootImpactEvent Event;
    Event.ImpactPoint = Hit.ImpactPoint;
    Event.ImpactNormal = Hit.ImpactNormal;
    Event.ImpulseMagnitude = ImpulseMagnitude;
    Event.Cause = PendingImpactCause;
    Event.LootActor = this;
    Event.HitActor = OtherActor;
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

    // 무엇에 부딪혔는지를 같이 찍는다. 임펄스만 봐서는 바닥에 떨어진 것인지
    // 운반자 몸에 튕긴 것인지 구분할 수 없다.
    ShowImpactDebug(
        FString::Printf(TEXT("확정 #%d  임펄스 %.0f  대상 %s  (OnHit 누적 %d회)"),
            DebugConfirmedCount, ImpulseMagnitude, *GetNameSafe(OtherActor), DebugRawHitCount),
        FColor::Yellow, Event.ImpactPoint, ImpulseMagnitude);

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

    // bConsumeInput 기본값이 true 라서, 그냥 두면 입력 스택 맨 위의 노획물 하나가
    // 키를 먹어 버리고 나머지는 아예 받지 못한다. 그 하나가 디버그 키를 켜 둔 액터라
    // "레벨에 있는 노획물 중 딱 하나만 집힌다" 는 증상이 된다.
    // 전부 받게 열어 두고, 중복 호출은 Debug_ClaimKeyPress 로 막는다.
    InputComponent->BindKey(EKeys::G, IE_Pressed,
        this, &ALootBase::Debug_ToggleGrabByLocalPlayer).bConsumeInput = false;

    // 누르고 있는 동안 조준, 뗄 때 던진다.
    InputComponent->BindKey(EKeys::T, IE_Pressed,
        this, &ALootBase::Debug_BeginThrowAim).bConsumeInput = false;
    InputComponent->BindKey(EKeys::T, IE_Released,
        this, &ALootBase::Debug_ThrowForward).bConsumeInput = false;
}

void ALootBase::Debug_ToggleGrabByLocalPlayer()
{
    static uint64 LastHandledFrame = MAX_uint64;
    if (!Debug_ClaimKeyPress(LastHandledFrame))
    {
        return;
    }

    APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsValid(LocalPawn))
    {
        // 에디터에서 (플레이 중이 아닐 때) 버튼이 눌린 경우.
        // 조용히 빠지면 원인을 못 찾으므로 로그라도 남긴다.
        UE_LOG(LogLoot, Warning, TEXT("[Loot:%s] 로컬 플레이어 폰이 없다. PIE 중인지 확인할 것."), *GetName());
        return;
    }

    // 월드를 훑어 '이번 입력의 대상' 하나를 고른 뒤, 그 액터에게 시킨다.
    // 자기 자신일 때만 움직이게 하면, 키를 받은 액터와 대상이 다를 때 아무 일도 일어나지 않는다.
    // 키를 받는 것과 집히는 것은 별개다 — 받은 쪽이 대리인 역할을 한다.
    ALootBase* Target = Debug_FindGrabTarget(LocalPawn);
    if (!IsValid(Target))
    {
        return;
    }

    if (Target->PrimaryCarrier.Get() == LocalPawn)
    {
        Target->OnReleased(LocalPawn);
        return;
    }

    Target->OnGrabbed(LocalPawn);
}

ALootBase* ALootBase::Debug_FindCarriedLoot(const APawn* LocalPawn) const
{
    // 들고 있는 것이 있으면 Debug_FindGrabTarget 이 그것을 먼저 돌려준다.
    ALootBase* Target = Debug_FindGrabTarget(LocalPawn);

    return (IsValid(Target) && Target->PrimaryCarrier.Get() == LocalPawn) ? Target : nullptr;
}

ALootBase* ALootBase::Debug_FindGrabTarget(const APawn* LocalPawn) const
{
    UWorld* World = GetWorld();
    if (!World || !IsValid(LocalPawn))
    {
        return nullptr;
    }

    ALootBase* Nearest = nullptr;
    float NearestDistSq = FMath::Square(DebugGrabRange);

    // 모든 노획물이 이 함수를 돌려 같은 답을 내야 한다.
    // (DebugGrabRange 를 노획물마다 다르게 두면 답이 갈린다. 테스트용이니 통일해서 쓸 것)
    for (TActorIterator<ALootBase> It(World); It; ++It)
    {
        ALootBase* Loot = *It;
        if (!IsValid(Loot))
        {
            continue;
        }

        // 이미 들고 있는 것이 있으면 그것이 대상이다 — 놓기가 먼저다.
        // 한 번에 하나만 들 수 있으므로 다른 것을 집으려면 먼저 놓아야 한다.
        if (Loot->PrimaryCarrier.Get() == LocalPawn)
        {
            return Loot;
        }

        // 남이 들고 있는 것은 후보에서 뺀다.
        if (IsValid(Loot->PrimaryCarrier))
        {
            continue;
        }

        const float DistSq =
            FVector::DistSquared(Loot->GetActorLocation(), LocalPawn->GetActorLocation());
        if (DistSq < NearestDistSq)
        {
            NearestDistSq = DistSq;
            Nearest = Loot;
        }
    }

    return Nearest;
}

FVector ALootBase::ComputeThrowAimDirection() const
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
    static uint64 LastHandledFrame = MAX_uint64;
    if (!Debug_ClaimKeyPress(LastHandledFrame))
    {
        return;
    }

    // G 와 마찬가지로 키를 받은 액터가 아니라 실제로 들고 있는 액터가 조준한다.
    ALootBase* Target = Debug_FindCarriedLoot(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!IsValid(Target))
    {
        return;
    }

    Target->bDebugAiming = true;
    Target->SetActorTickEnabled(true);
}

void ALootBase::Debug_ThrowForward()
{
    static uint64 LastHandledFrame = MAX_uint64;
    if (!Debug_ClaimKeyPress(LastHandledFrame))
    {
        return;
    }

    ALootBase* Target = Debug_FindCarriedLoot(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!IsValid(Target))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::White,
                TEXT("먼저 G 로 잡아야 한다"));
        }
        return;
    }

    APawn* Carrier = Target->PrimaryCarrier.Get();

    // 틱은 여기서 끄지 않는다. 소켓 없는 소지 중이면 위치 갱신에 틱이 계속 필요하고,
    // 그 판단은 Tick 이 스스로 한다. 여기서 끄면 던지지 못했을 때 물건이 시야에서 멈춘다.
    Target->bDebugAiming = false;

    // 조준 중 궤적과 같은 함수를 쓴다. 두 곳에서 따로 구하면 보던 것과 다르게 날아간다.
    const FVector AimDirection = Target->ComputeThrowAimDirection();
    if (AimDirection.IsNearlyZero())
    {
        return;
    }

    // 던지기 전에 그려야 한다. OnThrown 이 PrimaryCarrier 를 비우면
    // 운반자 속도가 빠져서 예측과 실제가 달라진다.
    // 조준 중 궤적은 한 프레임짜리라 사라지므로, 비교용으로 6초짜리를 한 번 더 남긴다.
    Target->ShowThrowTrajectory(AimDirection, 6.f);

    Target->OnThrown(Carrier, AimDirection);
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

void ALootBase::ShowImpactDebug(const FString& Message, const FColor& Color, const FVector& Location,
    float FilterImpulse) const
{
    if (!bShowImpactDebug)
    {
        return;
    }

    // 구르거나 미세하게 재접촉하는 것까지 전부 찍으면 정작 봐야 할 충돌이 묻힌다.
    // 음수는 임펄스와 무관한 메시지(버리기 위치 보정 등)라 걸러내지 않는다.
    if (FilterImpulse >= 0.f && FilterImpulse < DebugMinLogImpulse)
    {
        return;
    }

    UE_LOG(LogLoot, Log, TEXT("[Loot:%s] %s"), *GetName(), *Message);

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
