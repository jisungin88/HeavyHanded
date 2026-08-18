#include "Core/VanLoadZone.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include "Core/GameStates/HeistGameState.h"
#include "Core/HeavyHandedGameplayTags.h"
#include "Core/HeistLog.h"
#include "Core/HeistSettings.h"
#include "Loot/LootBase.h"

namespace
{
	/** Config/DefaultEngine.ini 의 프로파일 이름. 노획물만 Overlap 한다 */
	static const FName VanLoadZoneProfile(TEXT("VanLoadZone"));

	/** 기본 화물칸 크기(cm, 반지름). 밴 메시가 정해지면 인스턴스에서 조정한다 */
	constexpr float DefaultZoneExtent = 150.f;

	/**
	 * PendingLoot 의 값이 이것이면 아직 누가 들고 있다는 뜻이다.
	 *
	 * 별도 bool 을 두지 않는 이유는 두 값이 어긋날 수 없게 하기 위해서다 —
	 * "들려 있는데 체류 시각이 찍혀 있는" 상태가 아예 표현되지 않는다.
	 */
	constexpr float CarriedMarker = -1.f;
}

AVanLoadZone::AVanLoadZone()
{
	// 판정은 오버랩과 타이머로 돈다. 매 프레임 할 일이 없다.
	PrimaryActorTick.bCanEverTick = false;

	// 복제 프로퍼티는 하나도 없다. 그런데도 켜 두는 것은 확정 이펙트 멀티캐스트 때문이다 —
	// 복제하지 않는 액터는 RPC 를 보낼 수 없다.
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

	BorderDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("BorderDecal"));
	BorderDecal->SetupAttachment(LoadVolume);

	// 데칼은 자기 로컬 X 축 방향으로 투영한다. 바닥에 그리려면 그 축이 아래를 봐야 한다.
	BorderDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	// 크기는 볼륨에서 따라간다. 머티리얼이 없으면 OnConstruction 이 꺼 버린다.
	BorderDecal->SetVisibility(false);
}

void AVanLoadZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 볼륨을 늘리는 즉시 테두리도 같이 늘어나야 한다.
	// 배치 담당이 크기를 맞춰 놓고 게임을 켰더니 테두리만 옛 크기인 상황을 만들지 않는다.
	SyncBorderDecal();
}

void AVanLoadZone::BeginPlay()
{
	Super::BeginPlay();

	// 표시는 서버 · 클라이언트 모두 해야 한다. 권위 검사보다 위에 있는 이유다.
	if (!BorderMaterial)
	{
		DrawBorderFallback();
	}

	// 적재 판정은 서버 전용이다. 클라이언트에서 오버랩을 세면 사람마다 다른 금액이 나온다.
	if (!HasAuthority())
	{
		return;
	}

	WarnIfDuplicateZone();

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

void AVanLoadZone::WarnIfDuplicateZone() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 ZoneCount = 0;
	for (TActorIterator<AVanLoadZone> It(World); It; ++It)
	{
		++ZoneCount;
	}

	if (ZoneCount > 1)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("[VanLoadZone:%s] 레벨에 적재존이 %d개 있습니다. 진입 지역이 달라도 존은 하나여야 "
				 "합니다 — 어느 존이 금액을 올렸는지 추적할 수 없게 됩니다."),
			*GetName(), ZoneCount);
	}
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

	if (!IsLoadAllowedNow())
	{
		ShowLoadDebug(TEXT("거부 — 결과 페이즈에서는 적재하지 않는다"),
			FColor::Red, Loot->GetActorLocation());
		return;
	}

	// 확정 조건은 여기서 보지 않는다. 들려 있든 아니든 일단 추적을 시작하고,
	// 손을 떠난 시점부터 체류 시간을 재는 것은 재검사 쪽 일이다.
	TrackPending(Loot);
}

void AVanLoadZone::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	// IsValid 로 거르지 않는다. 확정 직후 파괴되는 노획물도 이 콜백을 한 번 만들고,
	// 그때는 이미 파괴 중이라 IsValid 가 false 다 — 목록에서 빼는 데는 포인터면 충분하다.
	ALootBase* Loot = Cast<ALootBase>(OtherActor);
	if (!Loot)
	{
		return;
	}

	// 들고 나갔거나, 튕겨 나갔거나, 확정돼서 사라졌다.
	// 다시 들어오면 체류 시간을 처음부터 다시 센다 — 굴러 나갔다 들어온 것은 새 사건이다.
	if (PendingLoot.Remove(Loot) > 0)
	{
		ShowLoadDebug(FString::Printf(TEXT("추적 해제 — %s 가 존 밖으로 나감"), *Loot->GetName()),
			FColor::Silver, Loot->GetActorLocation());

		UpdateRecheckTimer();
	}
}

// ──────────────────────────────────────────────────────────────
// 체류 시간 — 놓기와 던지기는 오버랩 이벤트를 만들지 않는다
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

void AVanLoadZone::TrackPending(ALootBase* Loot)
{
	// 이미 추적 중이면 손대지 않는다. 액터 하나가 여러 바디로 겹치면 BeginOverlap 이
	// 여러 번 오는데, 그때마다 시작 시각을 새로 찍으면 체류 시간이 영영 안 찬다.
	if (PendingLoot.Contains(Loot))
	{
		return;
	}

	// 이미 확정된 것이다. 기본 연출에서는 노획물이 곧 파괴되므로 여기 올 일이 없지만,
	// 연출을 NPC 로 바꿔 노획물을 살려 두면 그것이 존을 드나들 수 있다.
	// 금액을 두 번 세는 것이 이 시스템에서 낼 수 있는 최악의 결과라 상태로 막는다.
	if (Loot->HasMatchingGameplayTag(HHTags::Loot_State_Loaded))
	{
		ShowLoadDebug(FString::Printf(TEXT("거부 — %s 는 이미 실은 것이다"), *Loot->GetName()),
			FColor::Red, Loot->GetActorLocation());
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bCarried = IsValid(Loot->GetPrimaryCarrier());
	PendingLoot.Add(Loot, bCarried ? CarriedMarker : World->GetTimeSeconds());

	UpdateRecheckTimer();

	ShowLoadDebug(bCarried
		? FString::Printf(TEXT("대기 — %s 를 아직 %s 가 들고 있다"),
			*Loot->GetName(), *GetNameSafe(Loot->GetPrimaryCarrier()))
		: FString::Printf(TEXT("대기 — %s 체류 시간 시작"), *Loot->GetName()),
		FColor::Orange, Loot->GetActorLocation());
}

void AVanLoadZone::RecheckPending()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float DwellSeconds = UHeistSettings::Get()->LoadDwellSeconds;

	// 확정 처리가 PendingLoot 와 오버랩 상태를 건드리므로(파괴 → EndOverlap),
	// 순회 중에 바로 확정하면 반복자 아래에서 컨테이너가 바뀐다. 먼저 고르고 나중에 확정한다.
	TArray<ALootBase*> ToConfirm;

	for (auto It = PendingLoot.CreateIterator(); It; ++It)
	{
		ALootBase* Loot = It->Key.Get();

		// 추적 중에 파괴됐다 (파손형). 조용히 뺀다 — 없어진 물건은 실을 수 없다.
		if (!IsValid(Loot))
		{
			It.RemoveCurrent();
			continue;
		}

		// 다시 집어 들었다. 체류 시간은 손에서 떠난 순간부터 다시 센다.
		if (IsValid(Loot->GetPrimaryCarrier()))
		{
			It->Value = CarriedMarker;
			continue;
		}

		// 방금 손을 떠났다. 여기서부터가 시작이다.
		if (It->Value < 0.f)
		{
			It->Value = Now;
			continue;
		}

		if (Now - It->Value < DwellSeconds)
		{
			continue;   // 아직 채우는 중
		}

		It.RemoveCurrent();
		ToConfirm.Add(Loot);
	}

	for (ALootBase* Loot : ToConfirm)
	{
		// 체류하는 사이에 페이즈가 넘어갔을 수 있다. 페이즈 조건은 이 시점 기준으로 다시 본다.
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

	// 존 안에 아무것도 없으면 타이머를 멈춘다. 미션 대부분의 시간이 이 상태다.
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
// 확정
// ──────────────────────────────────────────────────────────────

void AVanLoadZone::ConfirmLoad(ALootBase* Loot)
{
	if (!HasAuthority() || !IsValid(Loot))
	{
		return;
	}

	// 액터가 사라져도 남아야 하는 것들을 먼저 값으로 뽑는다.
	const FHeistLoadEntry Entry = MakeLoadEntry(Loot);

	// 손에서 떠난 뒤에 확정되므로 PrimaryCarrier 는 이미 비어 있다.
	// 던져 넣기까지 포함해 "누가 실었는가" 를 아는 것은 이 값뿐이다.
	APawn* Loader = Loot->GetLastCarrier();

	// 정산 대상 표시. 확정 즉시 파괴하는 기본 연출에서는 수명이 짧지만,
	// 연출을 NPC 로 바꿔 노획물이 살아남게 되면 압력판 · 상호작용이 이 태그로 걸러낸다.
	Loot->SetLootStateTag(HHTags::Loot_State_Loaded);

	if (AHeistGameState* HeistState = AHeistGameState::Get(this))
	{
		HeistState->RecordLoadedLoot(Entry);
	}
	else
	{
		// 작업 레벨이 아닌 곳(테스트 맵)에 존만 놓은 경우다. 적재 자체는 되지만 집계가 없다.
		UE_LOG(LogHeist, Warning,
			TEXT("[VanLoadZone:%s] AHeistGameState 가 없어 금액을 집계하지 못했다 — %s ($%d)"),
			*GetName(), *Loot->GetName(), Entry.Value);
	}

	SendLoadedEvent(Loader, Loot, Entry.Value);

	UE_LOG(LogHeist, Log, TEXT("적재 확정 — %s ($%d), 적재자 %s"),
		*Loot->GetName(), Entry.Value,
		Loader ? *GetNameSafe(Loader) : TEXT("(알 수 없음)"));

	ShowLoadDebug(FString::Printf(TEXT("적재 확정 — %s ($%d)"), *Loot->GetName(), Entry.Value),
		FColor::Green, Loot->GetActorLocation());

	// 마지막이다. 기본 구현이 노획물을 파괴하므로 이 줄 아래에서 Loot 을 만지면 안 된다.
	HandleConfirmedLoot(Loot);
}

FHeistLoadEntry AVanLoadZone::MakeLoadEntry(const ALootBase* Loot) const
{
	FHeistLoadEntry Entry;

	if (!IsValid(Loot))
	{
		return Entry;
	}

	Entry.LootClass = Loot->GetClass();
	Entry.Value = Loot->GetCurrentValue();
	Entry.BaseValue = Loot->GetBaseValue();

	// 특성만 남기고 상태(Loot.State.*)는 뺀다. 결과 화면이 알아야 하는 것은
	// "무거운 것이었나 · 깨지는 것이었나" 이지 실리기 직전의 상태가 아니다.
	FGameplayTagContainer Owned;
	Loot->GetOwnedGameplayTags(Owned);
	Entry.TypeTags = Owned.Filter(FGameplayTagContainer(HHTags::Loot_Type.GetTag()));

	// 폰이 아니라 PlayerState 다. 폰은 체포 · 다운으로 파괴되지만 결과 화면은 그 뒤에 뜬다.
	if (const APawn* Loader = Loot->GetLastCarrier())
	{
		Entry.Loader = Loader->GetPlayerState();
	}

	return Entry;
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
// 연출 — 여기만 갈아 끼우면 NPC 적재가 된다
// ──────────────────────────────────────────────────────────────

void AVanLoadZone::HandleConfirmedLoot_Implementation(ALootBase* Loot)
{
	if (!IsValid(Loot))
	{
		return;
	}

	// 위치를 먼저 뽑는다. 파괴한 뒤에는 물어볼 대상이 없다.
	Multicast_PlayLoadedEffect(Loot->GetActorLocation());

	// 물리를 끄고 화물칸에 붙여 두지 않는다. 사라지는 것이 곧 "실렸다" 의 표현이고,
	// 남겨 두면 밴이 움직일 때 흘러내리는 것부터 클라이언트 물리까지 전부 우리 문제가 된다.
	Loot->Destroy();
}

void AVanLoadZone::Multicast_PlayLoadedEffect_Implementation(FVector Location)
{
	if (!LoadedEffect)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedEffect, Location);
}

// ──────────────────────────────────────────────────────────────
// 표시
// ──────────────────────────────────────────────────────────────

void AVanLoadZone::SyncBorderDecal()
{
	if (!BorderDecal || !LoadVolume)
	{
		return;
	}

	BorderDecal->SetDecalMaterial(BorderMaterial);
	BorderDecal->SetVisibility(BorderMaterial != nullptr);

	// 스케일은 컴포넌트가 부모에게서 함께 받으므로 여기서는 스케일 전 크기로 맞춘다.
	//
	// 데칼의 로컬 X 가 투영 방향(아래), Y 가 월드 Y, Z 가 월드 X 다 — 위 회전(Pitch -90) 때문이다.
	// X 에 볼륨 높이를 그대로 주면 존의 위아래를 전부 덮어, 밴 바닥이 어디에 있든 그려진다.
	const FVector Extent = LoadVolume->GetUnscaledBoxExtent();
	BorderDecal->DecalSize = FVector(Extent.Z, Extent.Y, Extent.X);
}

void AVanLoadZone::DrawBorderFallback() const
{
	UE_LOG(LogHeist, Warning,
		TEXT("[VanLoadZone:%s] BorderMaterial 이 없어 존 테두리를 디버그 선으로 그립니다. "
			 "머티리얼이 나오면 지정하세요."),
		*GetName());

#if ENABLE_DRAW_DEBUG
	if (const UWorld* World = GetWorld())
	{
		// 영구 선으로 한 번만 그린다. 존은 움직이지 않으므로 매 프레임 다시 그릴 이유가 없다.
		DrawDebugBox(World, GetActorLocation(), LoadVolume->GetScaledBoxExtent(), GetActorQuat(),
			FColor::Yellow, /*bPersistentLines=*/true, /*LifeTime=*/-1.f, /*DepthPriority=*/0,
			/*Thickness=*/3.f);
	}
#endif
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
