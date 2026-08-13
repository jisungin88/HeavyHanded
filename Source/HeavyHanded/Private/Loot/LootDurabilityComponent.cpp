#include "Loot/LootDurabilityComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Core/HeavyHandedTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Loot/LootBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ULootDurabilityComponent::ULootDurabilityComponent()
{
    // 충격은 이벤트로 온다. 매 프레임 확인할 것이 없다.
    PrimaryComponentTick.bCanEverTick = false;

    // ImpactCount / bIsBroken 이 복제되어야 클라이언트가 금 간 연출과 파괴를 맞출 수 있다.
    SetIsReplicatedByDefault(true);
}

void ULootDurabilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ULootDurabilityComponent, ImpactCount);
    DOREPLIFETIME(ULootDurabilityComponent, bIsBroken);
}

void ULootDurabilityComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerLoot = Cast<ALootBase>(GetOwner());
    if (!IsValid(OwnerLoot))
    {
        // 수치를 ALootBase 의 FLootPhysicsData 에서 읽으므로 다른 액터에는 붙을 수 없다.
        UE_LOG(LogTemp, Warning,
            TEXT("[%s] ULootDurabilityComponent 는 ALootBase 에만 붙일 수 있다. 파손 판정이 비활성화된다."),
            *GetNameSafe(GetOwner()));
        return;
    }

    // 확정 충격은 서버에서만 발생한다. 클라이언트는 복제된 값으로 연출만 맞춘다.
    if (OwnerLoot->HasAuthority())
    {
        ImpactDelegateHandle = OwnerLoot->OnLootImpact.AddUObject(
            this, &ULootDurabilityComponent::HandleLootImpact);
    }
}

void ULootDurabilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 구독을 남긴 채 파괴되면 죽은 객체를 호출한다.
    if (IsValid(OwnerLoot) && ImpactDelegateHandle.IsValid())
    {
        OwnerLoot->OnLootImpact.Remove(ImpactDelegateHandle);
        ImpactDelegateHandle.Reset();
    }

    if (const UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DestroyTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void ULootDurabilityComponent::HandleLootImpact(const FLootImpactEvent& Event)
{
    if (bIsBroken)
    {
        return;
    }

    // 파괴 방송을 다시 세면 자기 자신을 물고 들어간다.
    if (Event.Cause == ELootImpactCause::Break)
    {
        return;
    }

    // 사람 몸에 닿아서는 상하지 않는다. 넘어져서 바닥에 부딪히면 그때 상한다.
    // 키네마틱 캡슐은 살짝 닿아도 임펄스가 낙하의 몇 배로 튀어서, 임계값으로는 못 가른다.
    if (bIgnorePawnImpacts && Cast<APawn>(Event.HitActor.Get()))
    {
        if (OwnerLoot->IsImpactDebugEnabled() && GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Emerald,
                FString::Printf(TEXT("[%s] 파손 제외(사람 접촉) 임펄스 %.0f — %s"),
                    *OwnerLoot->GetName(), Event.ImpulseMagnitude, *GetNameSafe(Event.HitActor.Get())));
        }
        return;
    }

    const FLootPhysicsData& Data = OwnerLoot->GetPhysicsData();

    // 여기까지 온 충격은 이미 '소음으로 알릴 만한' 크기다.
    // 그렇다고 다 파손은 아니다. 파손 임계값은 따로 더 높게 잡혀 있다.
    if (Event.ImpulseMagnitude < Data.DamageImpulseThreshold)
    {
        return;
    }

    ++ImpactCount;

    // 서버에서 값을 직접 바꾸면 RepNotify 가 불리지 않는다. 서버 몫은 손으로 부른다.
    OnRep_ImpactCount();

    // 게이팅이 통과시킨 충격 중 실제로 파손까지 간 것이 몇 개인지 같이 봐야
    // DamageImpulseThreshold 가 적당한지 판단할 수 있다. 스위치는 ALootBase 와 공유한다.
    if (OwnerLoot->IsImpactDebugEnabled() && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red,
            FString::Printf(TEXT("[%s] 파손 %d / %d  (임펄스 %.0f)"),
                *OwnerLoot->GetName(), ImpactCount, Data.MaxImpactCount, Event.ImpulseMagnitude));
    }

    if (ImpactCount >= Data.MaxImpactCount)
    {
        Break(Event);
    }
}

void ULootDurabilityComponent::Break(const FLootImpactEvent& CausingEvent)
{
    bIsBroken = true;

    // 파괴는 부딪힌 소리와 별개의 사건이다. 상자가 바닥에 부딪히는 소리와
    // 깨지는 소리는 다르므로, 같은 충격에서 두 이벤트가 나가는 것이 맞다.
    // 여기서도 '깨졌다'는 물리적 사실만 알린다. 얼마나 시끄러운지는 소음 파트가 정한다.
    //
    // 액터를 지우기 전에 먼저 방송해야 한다. 구독자가 LootActor 를 유효한 상태로 받는다.
    OwnerLoot->ReportImpact(ELootImpactCause::Break, CausingEvent.ImpulseMagnitude,
        CausingEvent.ImpactPoint, CausingEvent.InstigatorPawn.Get());

    if (OwnerLoot->IsImpactDebugEnabled() && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Magenta,
            FString::Printf(TEXT("[%s] 파괴 — %.2f초 뒤 소멸"), *OwnerLoot->GetName(), BreakDestroyDelay));
    }

    ApplyBrokenState();

    // 바로 Destroy 하면 액터가 복제보다 먼저 사라져 클라이언트에서는 연출 없이 증발한다.
    // 이미 숨겨진 상태로 잠깐 남겨 두었다가 지운다.
    if (BreakDestroyDelay > 0.f)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(DestroyTimerHandle, this,
                &ULootDurabilityComponent::DestroyOwnerLoot, BreakDestroyDelay, false);
            return;
        }
    }

    DestroyOwnerLoot();
}

void ULootDurabilityComponent::ApplyBrokenState()
{
    if (!IsValid(OwnerLoot))
    {
        // 초기 복제는 BeginPlay 보다 먼저 도착할 수 있다. 그때는 여기서 해결한다.
        OwnerLoot = Cast<ALootBase>(GetOwner());
    }

    if (!IsValid(OwnerLoot))
    {
        return;
    }

    // bActorEnableCollision 은 복제되지 않는다. 각 머신에서 따로 꺼야 한다.
    // (bHidden 은 복제되지만, 클라이언트도 즉시 반영하도록 여기서 같이 처리한다)
    OwnerLoot->SetActorEnableCollision(false);

    if (UPrimitiveComponent* PhysicsRoot = OwnerLoot->GetPhysicsRoot())
    {
        PhysicsRoot->SetSimulatePhysics(false);
    }

    OwnerLoot->SetActorHiddenInGame(true);

    // 파편·사운드는 BP 껍데기가 담당한다. 판정은 이미 끝났다.
    OnBroken();
}

void ULootDurabilityComponent::DestroyOwnerLoot()
{
    // 파괴는 서버 권위다. 클라이언트는 액터가 사라지는 것을 복제로 받는다.
    if (IsValid(OwnerLoot) && OwnerLoot->HasAuthority())
    {
        OwnerLoot->Destroy();
    }
}

void ULootDurabilityComponent::OnRep_ImpactCount()
{
    if (!IsValid(OwnerLoot))
    {
        // 초기 복제는 BeginPlay 보다 먼저 도착할 수 있다. 그때는 여기서 해결한다.
        OwnerLoot = Cast<ALootBase>(GetOwner());
    }

    const int32 MaxCount = IsValid(OwnerLoot) ? OwnerLoot->GetPhysicsData().MaxImpactCount : 0;
    OnDamageAccumulated(ImpactCount, MaxCount);
}

void ULootDurabilityComponent::OnRep_IsBroken()
{
    if (bIsBroken)
    {
        ApplyBrokenState();
    }
}
