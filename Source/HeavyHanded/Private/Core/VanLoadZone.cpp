#include "Core/VanLoadZone.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistLog.h"
#include "Loot/LootBase.h"

namespace
{
	/** Config/DefaultEngine.ini 의 프로파일 이름. 노획물만 Overlap 한다 */
	static const FName VanLoadZoneProfile(TEXT("VanLoadZone"));

	/** 기본 화물칸 크기(cm, 반지름). 밴 메시가 정해지면 인스턴스에서 조정한다 */
	constexpr float DefaultZoneExtent = 150.f;
}

AVanLoadZone::AVanLoadZone()
{
	// 판정은 오버랩과 타이머로 돈다. 매 프레임 할 일이 없다.
	PrimaryActorTick.bCanEverTick = false;

	// LoadedLoot 를 클라이언트에 보내야 화물이 각자 화면에서 같은 자리에 붙어 있는다.
	bReplicates = true;

	// 밴 자체가 움직이게 되면(도주 연출) 그때 이 값을 켠다. 지금은 정지해 있어 낭비다.
	SetReplicateMovement(false);

	LoadVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("LoadVolume"));
	SetRootComponent(LoadVolume);

	LoadVolume->SetBoxExtent(FVector(DefaultZoneExtent));
	LoadVolume->SetCollisionProfileName(VanLoadZoneProfile);
	LoadVolume->SetGenerateOverlapEvents(true);

	// 판정 볼륨이지 화물칸 벽이 아니다. 게임 화면에는 보이지 않아야 한다.
	LoadVolume->SetHiddenInGame(true);
}

void AVanLoadZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AVanLoadZone, LoadedLoot);
}

void AVanLoadZone::BeginPlay()
{
	Super::BeginPlay();

	// 적재 판정은 서버 전용이다. 클라이언트에서 오버랩을 세면 사람마다 다른 금액이 나온다.
	// 클라이언트가 할 일은 OnRep_LoadedLoot 로 결과를 따라가는 것뿐이라 델리게이트를 붙이지 않는다.
	if (!HasAuthority())
	{
		return;
	}

	LoadVolume->OnComponentBeginOverlap.AddDynamic(this, &AVanLoadZone::HandleBeginOverlap);
	LoadVolume->OnComponentEndOverlap.AddDynamic(this, &AVanLoadZone::HandleEndOverlap);

	// 레벨에 처음부터 존 안에 놓여 있는 노획물은 오버랩 이벤트를 만들지 않는다.
	// (엔진이 BeginPlay 전에 겹침을 갱신해 버려서 콜백을 받을 시점이 지나 있다)
	// 배치 실수로 밴 안에 놓인 노획물이 조용히 무시되면 원인을 찾기 어려우므로 한 번 훑는다.
	TArray<AActor*> AlreadyInside;
	LoadVolume->GetOverlappingActors(AlreadyInside, ALootBase::StaticClass());
	for (AActor* Actor : AlreadyInside)
	{
		HandleBeginOverlap(LoadVolume, Actor, nullptr, INDEX_NONE, /*bFromSweep=*/false, FHitResult());
	}
}

void AVanLoadZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환 중에 재검사가 돌면 이미 정리되기 시작한 월드를 건드린다.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecheckTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// ──────────────────────────────────────────────────────────────
// 오버랩 — 존 안에 들어왔는가
// ──────────────────────────────────────────────────────────────

void AVanLoadZone::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	ALootBase* Loot = Cast<ALootBase>(OtherActor);
	if (!IsValid(Loot))
	{
		return;
	}

	// 이미 실은 것이다. 어태치 상태라 여기 다시 올 일이 없어야 하지만,
	// 들어왔다면 두 번 세는 것이 최악이므로 금액을 만지기 전에 막는다.
	if (LoadedLoot.Contains(Loot))
	{
		ShowLoadDebug(FString::Printf(TEXT("거부 — %s 는 이미 실은 것이다"), *Loot->GetName()),
			FColor::Red, Loot->GetActorLocation());
		return;
	}

	if (!IsLoadAllowedNow())
	{
		ShowLoadDebug(TEXT("거부 — 결과 페이즈에서는 적재하지 않는다"),
			FColor::Red, Loot->GetActorLocation());
		return;
	}

	// 들려 있으면 확정하지 않는다. 손에서 떠나야 적재다.
	// 거부가 아니라 보류다 — 그 자리에서 놓으면 다음 재검사에서 확정된다.
	if (IsValid(Loot->GetPrimaryCarrier()))
	{
		HoldForRecheck(Loot);
		return;
	}

	ConfirmLoad(Loot);
}

void AVanLoadZone::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	ALootBase* Loot = Cast<ALootBase>(OtherActor);
	if (!IsValid(Loot))
	{
		return;
	}

	// 들고 나갔거나 튕겨 나갔다. 다시 들어오면 그때 처음부터 판정한다.
	//
	// 확정 직후에도 이 콜백이 온다 — ApplyLoadedState 가 콜리전을 끄면서 겹침이 풀리기 때문이다.
	// 그때 Loot 은 이미 보류 목록에 없으므로 Remove 는 아무 일도 하지 않는다.
	if (PendingLoot.Remove(Loot) > 0)
	{
		ShowLoadDebug(FString::Printf(TEXT("보류 해제 — %s 가 존 밖으로 나감"), *Loot->GetName()),
			FColor::Silver, Loot->GetActorLocation());

		UpdateRecheckTimer();
	}
}

// ──────────────────────────────────────────────────────────────
// 확정 판정
// ──────────────────────────────────────────────────────────────

bool AVanLoadZone::IsLoadAllowedNow() const
{
	// 결과 화면이 뜬 뒤에 굴러 들어온 물건까지 금액에 반영되면, 화면에 이미 표시된 정산액과
	// GameState 의 값이 달라진다. Prep · Heist · Escape 는 전부 받는다 —
	// 준비 시간에 미리 실어 두는 것도 플레이어의 선택이다.
	//
	// 작업 레벨이 아닌 테스트 맵에는 AHeistGameState 가 없다. 그때는 막지 않는다 —
	// 페이즈가 없다는 것은 "결과 화면이 떴다" 가 아니라 "판정할 근거가 없다" 는 뜻이다.
	const AHeistGameState* HeistState = AHeistGameState::Get(this);

	return !HeistState || !HeistState->IsPhase(HHTags::Phase_Result);
}

void AVanLoadZone::HoldForRecheck(ALootBase* Loot)
{
	PendingLoot.Add(Loot);
	UpdateRecheckTimer();

	ShowLoadDebug(FString::Printf(TEXT("보류 — %s 를 아직 %s 가 들고 있다"),
		*Loot->GetName(), *GetNameSafe(Loot->GetPrimaryCarrier())),
		FColor::Orange, Loot->GetActorLocation());
}

void AVanLoadZone::ConfirmLoad(ALootBase* Loot)
{
	if (!HasAuthority() || !IsValid(Loot))
	{
		return;
	}

	// 가치는 노획물이 정한다. 파손·유출이 이미 반영된 값이라 여기서 다시 계산하지 않는다.
	const int32 LoadedValue = Loot->GetCurrentValue();

	// 손에서 떠난 뒤에 확정되므로 PrimaryCarrier 는 이미 비어 있다.
	// 던져 넣기까지 포함해 "누가 실었는가" 를 아는 것은 이 값뿐이다.
	APawn* Loader = Loot->GetLastCarrier();

	// 복제 대상이라 클라이언트가 같은 모습을 만든다. 서버 몫은 손으로 부른다.
	LoadedLoot.Add(Loot);
	ApplyLoadedState(Loot);

	// 정산 대상 표시. 압력판·상호작용처럼 상태를 보는 쪽이 적재된 물건을 걸러낼 수 있게 한다.
	Loot->SetLootStateTag(HHTags::Loot_State_Loaded);

	if (AHeistGameState* HeistState = AHeistGameState::Get(this))
	{
		HeistState->AddLoadedValue(LoadedValue);
	}
	else
	{
		// 작업 레벨이 아닌 곳(테스트 맵)에 존만 놓은 경우다. 적재 자체는 되지만 집계가 없다.
		UE_LOG(LogHeist, Warning,
			TEXT("[VanLoadZone:%s] AHeistGameState 가 없어 금액을 집계하지 못했다 — %s ($%d)"),
			*GetName(), *Loot->GetName(), LoadedValue);
	}

	SendLoadedEvent(Loader, Loot, LoadedValue);

	UE_LOG(LogHeist, Log, TEXT("적재 확정 — %s ($%d), 적재자 %s"),
		*Loot->GetName(), LoadedValue,
		Loader ? *GetNameSafe(Loader) : TEXT("(알 수 없음)"));

	ShowLoadDebug(FString::Printf(TEXT("적재 확정 — %s ($%d)"), *Loot->GetName(), LoadedValue),
		FColor::Green, Loot->GetActorLocation());
}

void AVanLoadZone::ApplyLoadedState(ALootBase* Loot) const
{
	if (!IsValid(Loot))
	{
		return;
	}

	USceneComponent* AttachTarget = GetAttachTarget();
	if (!AttachTarget)
	{
		return;
	}

	if (UPrimitiveComponent* Body = Loot->GetPhysicsRoot())
	{
		// 어태치보다 먼저 꺼야 한다. 시뮬레이션 중인 바디는 부모를 따라가지 않고
		// 자기 물리 결과를 그대로 밀어붙여서, 밴이 움직이면 화물만 제자리에 남는다.
		Body->SetSimulatePhysics(false);

		// 화물칸에 물건이 쌓일수록 밴에 타려는 플레이어가 막힌다. 실린 뒤로는 장식이다.
		// 존과의 겹침도 여기서 풀려 오버랩 콜백이 더 오지 않는다.
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 떨어진 자리에 그대로 둔다. 소켓에 정렬하면 던져 넣은 물건이 순간이동해서
	// "들어갔다" 는 물리적 결과가 화면에서 지워진다.
	Loot->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepWorldTransform);
}

void AVanLoadZone::SendLoadedEvent(APawn* Loader, ALootBase* Loot, int32 LoadedValue) const
{
	if (!IsValid(Loader))
	{
		// 아무도 든 적 없이 굴러 들어온 경우다. 금액은 이미 들어갔고 기여도만 주인이 없다.
		return;
	}

	// 페이로드 규약은 HHTags::Event_Loot_Loaded 주석에 있다. 받는 쪽이 그 약속에 기대므로 맞출 것.
	FGameplayEventData Payload;
	Payload.EventTag = HHTags::Event_Loot_Loaded;
	Payload.Instigator = Loot;
	Payload.Target = Loader;
	Payload.OptionalObject = Loot;
	Payload.EventMagnitude = static_cast<float>(LoadedValue);

	// 대상이 IAbilitySystemInterface 가 아니면 이 함수가 조용히 빠진다.
	// 폰이 아니라 PlayerState 가 ASC 를 들고 있어도(APlayerSessionState) ABaseCharacter 가
	// 인터페이스로 넘겨주므로 여기서 그 구조를 알 필요가 없다.
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Loader, HHTags::Event_Loot_Loaded, Payload);
}

// ──────────────────────────────────────────────────────────────
// 보류 재검사 — 놓기는 오버랩 이벤트를 만들지 않는다
// ──────────────────────────────────────────────────────────────

void AVanLoadZone::RecheckPending()
{
	// 확정 처리가 PendingLoot 와 오버랩 상태를 건드리므로(콜리전 OFF → EndOverlap),
	// 순회 중에 바로 확정하면 반복자 아래에서 컨테이너가 바뀐다. 먼저 고르고 나중에 확정한다.
	TArray<ALootBase*> ToConfirm;

	for (auto It = PendingLoot.CreateIterator(); It; ++It)
	{
		ALootBase* Loot = It->Get();

		// 보류 중에 파괴됐다 (파손형). 조용히 뺀다 — 없어진 물건은 실을 수 없다.
		if (!IsValid(Loot))
		{
			It.RemoveCurrent();
			continue;
		}

		// 아직 들고 있다. 계속 기다린다.
		if (IsValid(Loot->GetPrimaryCarrier()))
		{
			continue;
		}

		It.RemoveCurrent();
		ToConfirm.Add(Loot);
	}

	for (ALootBase* Loot : ToConfirm)
	{
		// 들고 서 있는 사이에 페이즈가 넘어갔을 수 있다. 페이즈 조건은 이 시점 기준으로 다시 본다.
		if (IsLoadAllowedNow())
		{
			ConfirmLoad(Loot);
		}
	}

	UpdateRecheckTimer();
}

void AVanLoadZone::UpdateRecheckTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();

	// 검사할 것이 없으면 타이머를 멈춘다. 화물칸에 물건이 쌓여 있어도
	// 돌아야 하는 것은 '지금 들려 있는 것' 이 있을 때뿐이다.
	if (PendingLoot.IsEmpty())
	{
		Timers.ClearTimer(RecheckTimerHandle);
		return;
	}

	if (!Timers.IsTimerActive(RecheckTimerHandle))
	{
		Timers.SetTimer(RecheckTimerHandle, this, &AVanLoadZone::RecheckPending,
			RecheckIntervalSeconds, /*bLoop=*/true);
	}
}

// ──────────────────────────────────────────────────────────────
// 클라이언트 반영
// ──────────────────────────────────────────────────────────────

void AVanLoadZone::OnRep_LoadedLoot()
{
	// 목록 전체를 다시 적용한다. 이미 적용된 것에 다시 걸어도 결과가 같아서
	// "어디까지 처리했는가" 를 따로 들고 있을 필요가 없다.
	//
	// 늦게 들어온 클라이언트에서는 노획물 액터가 아직 도착하지 않아 항목이 비어 올 수 있다.
	// 그때 엔진이 참조가 풀리는 시점에 이 RepNotify 를 다시 불러 주므로 여기서는 건너뛰면 된다.
	for (ALootBase* Loot : LoadedLoot)
	{
		if (IsValid(Loot))
		{
			ApplyLoadedState(Loot);
		}
	}
}

USceneComponent* AVanLoadZone::GetAttachTarget() const
{
	if (IsValid(VanActor) && VanActor->GetRootComponent())
	{
		return VanActor->GetRootComponent();
	}

	return GetRootComponent();
}

void AVanLoadZone::ShowLoadDebug(const FString& Message, const FColor& Color, const FVector& Location) const
{
	if (!bShowLoadDebug)
	{
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("[VanLoadZone:%s] %s"), *GetName(), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, Color,
			FString::Printf(TEXT("[%s] %s"), *GetName(), *Message));
	}

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), Location, 20.f, 12, Color, false, 3.f);
#endif
}
