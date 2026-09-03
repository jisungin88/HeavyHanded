#include "Core/VanZone.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	/** 같은 파일의 승차 판정 프로파일. 플레이어만 Overlap 한다 */
	static const FName VanBoardZoneProfile(TEXT("VanBoardZone"));

	/** 기본 화물칸 크기(cm, 반지름). 밴 메시가 정해지면 인스턴스에서 조정한다 */
	constexpr float DefaultZoneExtent = 150.f;

	/** 뒷문 개구부 높이(cm). 조준 판의 크기와 하차 지점 기준이 된다 */
	constexpr float DoorHeight = 200.f;

	/**
	 * 테두리 데칼의 투영 깊이에 더하는 여유(cm).
	 * 볼륨 높이에 딱 맞추면 밴이 지면에서 몇 cm만 떠도 테두리가 통째로 사라진다.
	 */
	constexpr float BorderDepthMargin = 50.f;

	/** 테두리 폴백을 다시 그리는 주기(초) */
	constexpr float BorderDebugRedrawSeconds = 0.5f;

	/** 그려진 선이 남아 있는 시간(초). 재그리기 주기보다 길어야 깜빡이지 않는다 */
	constexpr float BorderDebugLifetimeSeconds = BorderDebugRedrawSeconds * 1.2f;

	/** 하차 지점을 문짝에서 얼마나 더 바깥에 둘 것인가(cm) */
	constexpr float ExitOffsetBeyondDoor = 80.f;

	/** 좌석 수. 모자라면 겹쳐 앉히므로 이 값이 틀려도 누가 못 타지는 않는다 */
	constexpr int32 MaxSeats = 4;

	/**
	 * PendingLoot 의 값이 이것이면 아직 누가 들고 있다는 뜻이다.
	 * 별도 bool 을 두지 않는 것은 "들려 있는데 체류 시각이 찍힌" 상태를 표현 불가능하게 하려는 것.
	 */
	constexpr float CarriedMarker = -1.f;
}

AVanZone::AVanZone()
{
	PrimaryActorTick.bCanEverTick = false;

	// 복제 프로퍼티는 하나도 없다. 그런데도 켜는 것은 확정 이펙트 멀티캐스트 때문이다 —
	// 복제하지 않는 액터는 RPC 를 보낼 수 없다
	bReplicates = true;

	// 레벨 배치 액터라 반드시 켜야 한다. 클라이언트는 레벨 파일에 저장된 위치를 쓰므로,
	// 꺼 두면 서버만 밴을 옮기고 **클라이언트에서만 밴이 제자리에 남는다**
	SetReplicateMovement(true);

	VanRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VanRoot"));
	SetRootComponent(VanRoot);

	LoadVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("LoadVolume"));
	LoadVolume->SetupAttachment(VanRoot);

	LoadVolume->SetBoxExtent(FVector(DefaultZoneExtent));

	// 기본 크기 기준으로 바닥이 VanRoot 평면에 닿는 높이
	LoadVolume->SetRelativeLocation(FVector(0.f, 0.f, DefaultZoneExtent));
	LoadVolume->SetCollisionProfileName(VanLoadZoneProfile);
	LoadVolume->SetGenerateOverlapEvents(true);
	LoadVolume->SetHiddenInGame(true);

	BoardVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardVolume"));
	BoardVolume->SetupAttachment(VanRoot);

	BoardVolume->SetBoxExtent(FVector(DefaultZoneExtent));
	BoardVolume->SetRelativeLocation(FVector(0.f, 0.f, DefaultZoneExtent));
	BoardVolume->SetCollisionProfileName(VanBoardZoneProfile);
	BoardVolume->SetHiddenInGame(true);

	// 콜백은 걸지 않지만 겹침 추적은 켜야 한다. IsOverlappingActor 가 이 목록을 읽는다
	BoardVolume->SetGenerateOverlapEvents(true);

	BoardAimTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardAimTarget"));
	BoardAimTarget->SetupAttachment(VanRoot);

	// 박스는 중심 기준이라 높이의 절반만큼 올려야 바닥에 선다
	BoardAimTarget->SetRelativeLocation(FVector(DefaultZoneExtent, 0.f, DoorHeight * 0.5f));
	BoardAimTarget->SetBoxExtent(FVector(5.f, 70.f, DoorHeight * 0.5f));

	BoardAimTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardAimTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardAimTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoardAimTarget->SetHiddenInGame(true);

	// Z 가 0 인 것은 앵커가 '발이 닿는 지점' 이고 VanRoot 가 바닥이기 때문이다
	static const FVector DefaultSeatOffsets[] = {
		FVector(-60.f, -60.f, 0.f), FVector(-60.f,  60.f, 0.f),
		FVector( 30.f, -60.f, 0.f), FVector( 30.f,  60.f, 0.f)
	};

	Seats.Reserve(MaxSeats);
	for (int32 Index = 0; Index < MaxSeats; ++Index)
	{
		USceneComponent* Seat = CreateDefaultSubobject<USceneComponent>(
			*FString::Printf(TEXT("Seat%d"), Index));
		Seat->SetupAttachment(VanRoot);
		Seat->SetRelativeLocation(DefaultSeatOffsets[Index]);

		Seats.Add(Seat);
	}

	ExitAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ExitAnchor"));
	ExitAnchor->SetupAttachment(VanRoot);
	ExitAnchor->SetRelativeLocation(
		FVector(DefaultZoneExtent + ExitOffsetBeyondDoor, 0.f, 0.f));

	// 볼륨에 붙이지 않는다 — 회전이 딸려 와서 투영 축이 같이 돌아간다
	BorderDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("BorderDecal"));
	BorderDecal->SetupAttachment(VanRoot);

	// 에디터에서 잠깐 보이는 기본값일 뿐이다. 실제 값은 SyncBorderDecal 이 정한다
	BorderDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	BorderDecal->SetVisibility(false);
}

void AVanZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 볼륨을 늘리는 즉시 테두리도 같이 늘어나야 한다
	SyncBorderDecal();
}

void AVanZone::BeginPlay()
{
	Super::BeginPlay();

	// 표시는 서버 · 클라이언트 모두 해야 한다. 권위 검사보다 위에 있는 이유다
	if (!BorderMaterial)
	{
		// 경고는 여기서 한 번만 낸다. 아래 재그리기는 반복 호출이라 같이 두면 로그가 잠긴다
		UE_LOG(LogHeist, Warning,
			TEXT("[VanZone:%s] BorderMaterial 이 없어 존 테두리를 디버그 선으로 그립니다. "
				 "머티리얼이 나오면 지정하세요."),
			*GetName());

#if ENABLE_DRAW_DEBUG
		DrawBorderFallback();

		GetWorldTimerManager().SetTimer(BorderDebugTimerHandle, this,
			&AVanZone::DrawBorderFallback, BorderDebugRedrawSeconds, /*bLoop=*/true);
#endif
	}

	// 적재 판정은 서버 전용이다. 클라이언트에서 세면 사람마다 다른 금액이 나온다
	if (!HasAuthority())
	{
		return;
	}

	WarnIfDuplicateZone();

	LoadVolume->OnComponentBeginOverlap.AddDynamic(this, &AVanZone::HandleBeginOverlap);
	LoadVolume->OnComponentEndOverlap.AddDynamic(this, &AVanZone::HandleEndOverlap);

	// 레벨에 처음부터 존 안에 놓인 노획물은 오버랩 이벤트를 만들지 않는다
	// (엔진이 BeginPlay 전에 겹침을 갱신해 콜백 시점이 이미 지나 있다). 한 번 훑는다
	TArray<AActor*> AlreadyInside;
	LoadVolume->GetOverlappingActors(AlreadyInside, ALootBase::StaticClass());
	for (AActor* Actor : AlreadyInside)
	{
		HandleBeginOverlap(LoadVolume, Actor, nullptr, INDEX_NONE, /*bFromSweep=*/false, FHitResult());
	}
}

void AVanZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환 중에 재검사가 돌면 이미 정리되기 시작한 월드를 건드린다
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecheckTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AVanZone::WarnIfDuplicateZone() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 ZoneCount = 0;
	for (TActorIterator<AVanZone> It(World); It; ++It)
	{
		++ZoneCount;
	}

	if (ZoneCount > 1)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("[VanZone:%s] 레벨에 적재존이 %d개 있습니다. 진입 지역이 달라도 존은 하나여야 "
				 "합니다 — 어느 존이 금액을 올렸는지 추적할 수 없게 됩니다."),
			*GetName(), ZoneCount);
	}
}

// ──────────────────────────────────────────────────────────────
// 오버랩
// ──────────────────────────────────────────────────────────────

void AVanZone::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
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

	// 들려 있든 아니든 일단 추적을 시작한다. 체류 시간은 재검사 쪽 일이다
	TrackPending(Loot);
}

void AVanZone::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	// IsValid 로 거르지 않는다. 확정 직후 파괴되는 노획물도 이 콜백을 한 번 만드는데,
	// 그때는 이미 파괴 중이라 IsValid 가 false 다 — 목록에서 빼는 데는 포인터면 충분하다
	ALootBase* Loot = Cast<ALootBase>(OtherActor);
	if (!Loot)
	{
		return;
	}

	// 다시 들어오면 체류 시간을 처음부터 다시 센다 — 굴러 나갔다 들어온 것은 새 사건이다
	if (PendingLoot.Remove(Loot) > 0)
	{
		ShowLoadDebug(FString::Printf(TEXT("추적 해제 — %s 가 존 밖으로 나감"), *Loot->GetName()),
			FColor::Silver, Loot->GetActorLocation());

		UpdateRecheckTimer();
	}
}

// ──────────────────────────────────────────────────────────────
// 승차
// ──────────────────────────────────────────────────────────────

AVanZone* AVanZone::Get(const UObject* WorldContext)
{
	if (!GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	// 둘 이상이면 BeginPlay 가 이미 경고했으므로 여기서 다시 따지지 않는다
	for (TActorIterator<AVanZone> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

bool AVanZone::TryDisembarkIfBoarded(APawn* Player)
{
	if (!IsValid(Player))
	{
		return false;
	}

	const APlayerState* PlayerState = Player->GetPlayerState();
	if (!PlayerState)
	{
		return false;
	}

	// 명단을 먼저 본다. 타고 있지 않은 경우가 압도적으로 흔한데 그때마다 액터를 훑을 이유가 없다
	const AHeistGameState* HeistState = AHeistGameState::Get(Player);
	if (!HeistState || !HeistState->IsBoarded(PlayerState))
	{
		return false;
	}

	AVanZone* Van = Get(Player);
	return Van && Van->TryToggleBoarding(Player);
}

bool AVanZone::TryToggleBoarding(APawn* Player)
{
	// 클라이언트가 자기 명단을 고칠 수 있으면 탈출 판정이 곧 치트가 된다
	if (!HasAuthority() || !IsValid(Player))
	{
		return false;
	}

	// 조종자가 없는 폰은 명단에 오를 수 없다. 사람으로 세면 인원이 맞지 않는다
	APlayerState* PlayerState = Player->GetPlayerState();
	if (!PlayerState)
	{
		return false;
	}

	AHeistGameState* HeistState = AHeistGameState::Get(this);
	if (!HeistState)
	{
		// 작업 레벨이 아닌 테스트 맵이다. 적재는 되지만 승차는 셀 곳이 없다
		return false;
	}

	// 명단은 이미 얼어 있어 집계는 안전하다. 그런데도 막는 것은 몸 때문이다 —
	// 탈출로 남은 사람이 밴에서 걸어 나오면 결과 화면과 눈앞의 장면이 어긋난다
	if (HeistState->IsPhase(HHTags::Phase_Result))
	{
		ShowLoadDebug(TEXT("거부 — 결과 화면에서는 타고 내릴 수 없다"),
			FColor::Red, Player->GetActorLocation());
		return false;
	}

	const bool bWasBoarded = HeistState->IsBoarded(PlayerState);

	// 내리는 것은 (결과 화면을 빼면) 언제나 된다. 탈 때만 조건을 본다
	if (!bWasBoarded && !CanBoardNow(Player))
	{
		return false;
	}

	const bool bBoarded = !bWasBoarded;

	// 폰을 먼저 처리한다. 명단 갱신이 GameMode 의 종료 판정을 그 자리에서 불러서,
	// 순서를 뒤집으면 결과 화면이 뜬 뒤에 폰이 붙는다
	ApplyBoardedPawnState(Player, bBoarded);
	HeistState->SetBoarded(PlayerState, bBoarded);

	ShowLoadDebug(FString::Printf(TEXT("%s — %s (승차 %d명)"),
		bBoarded ? TEXT("승차") : TEXT("하차"),
		*PlayerState->GetPlayerName(), HeistState->GetBoardedNum()),
		bBoarded ? FColor::Cyan : FColor::Silver, Player->GetActorLocation());

	// 이벤트는 "일어난 일" 이라 탈 때만 보낸다. 하차는 State.InVan 이 떨어지는 것으로 표현된다
	if (bBoarded)
	{
		SendBoardedEvent(Player, HeistState->GetBoardedNum());
	}

	return true;
}

void AVanZone::OnInteract_Implementation(APawn* Interactor)
{
	// 하차는 PerformInteraction 이 스윕보다 먼저 TryDisembarkIfBoarded 로 처리한다.
	// 여기로 들어오는 것은 전부 "밖에서 조준해 태워 달라" 는 경우다
	TryToggleBoarding(Interactor);
}

bool AVanZone::CanBoardNow(const APawn* Player) const
{
	// 어빌리티 사거리(기본 300cm)만으로는 밴 옆이나 지붕 위에서도 닿는다
	if (!BoardVolume->IsOverlappingActor(Player))
	{
		ShowLoadDebug(FString::Printf(TEXT("승차 거부 — %s 가 뒷칸 밖에 있다"),
			*GetNameSafe(Player)), FColor::Red, Player->GetActorLocation());
		return false;
	}

	const AHeistGameState* HeistState = AHeistGameState::Get(this);
	if (!HeistState)
	{
		return true;   // 페이즈가 없는 테스트 맵. 판정할 근거가 없으면 막지 않는다
	}

	// 준비 시간에 이동이 묶이면 그 시간을 통째로 날린다
	if (!HeistState->IsPhase(HHTags::Phase_Heist) && !HeistState->IsPhase(HHTags::Phase_Escape))
	{
		ShowLoadDebug(TEXT("승차 거부 — 본 작업이 시작되기 전이다"),
			FColor::Red, Player->GetActorLocation());
		return false;
	}

	return true;
}

void AVanZone::ApplyBoardedPawnState(APawn* Player, bool bBoarded)
{
	ACharacter* Character = Cast<ACharacter>(Player);
	if (!Character)
	{
		return;
	}

	APlayerState* PlayerState = Player->GetPlayerState();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	if (bBoarded)
	{
		if (USceneComponent* Seat = TakeSeat(PlayerState))
		{
			// 회전은 건드리지 않는다 — 컨트롤러가 매 프레임 덮어쓴다
			PlaceAtAnchor(Character, Seat);

			Character->AttachToComponent(Seat, FAttachmentTransformRules::KeepWorldTransform);
		}
		else
		{
			// 좌석이 없으면 서 있던 자리에 고정한다. 루트가 아니라 볼륨에 붙이는 것은
			// 밴이 움직이게 될 때 "화물칸에 실려 간다" 가 되어야 하기 때문이다
			Character->AttachToComponent(LoadVolume,
				FAttachmentTransformRules::KeepWorldTransform);
		}

		if (Movement)
		{
			// MovementMode 는 복제되므로 클라이언트도 같이 멈춘다
			Movement->DisableMovement();
			Movement->StopMovementImmediately();
		}
	}
	else
	{
		ReleaseSeat(PlayerState);

		// 떼어낸 뒤에 옮긴다. 붙어 있는 채로 위치를 바꾸면 어태치가 상대 좌표로 다시 해석한다
		Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		PlaceAtAnchor(Character, ExitAnchor);

		if (Movement)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

// ──────────────────────────────────────────────────────────────
// 좌석
// ──────────────────────────────────────────────────────────────

USceneComponent* AVanZone::TakeSeat(APlayerState* Player)
{
	if (Seats.IsEmpty() || !Player)
	{
		return nullptr;
	}

	// 좌석 수가 바뀌어도 장부가 따라오게 매번 맞춘다
	SeatOccupants.SetNum(Seats.Num());

	const AHeistGameState* HeistState = AHeistGameState::Get(this);

	int32 FirstFree = INDEX_NONE;

	for (int32 Index = 0; Index < SeatOccupants.Num(); ++Index)
	{
		const APlayerState* Occupant = SeatOccupants[Index].Get();

		if (Occupant == Player)
		{
			return Seats[Index];   // 이미 앉아 있다. 자리를 옮기지 않는다
		}

		if (FirstFree != INDEX_NONE)
		{
			continue;
		}

		// 비었는가를 '상태' 가 아니라 '사실' 로 본다 — 하차 처리를 놓치면 자리가 영영 잠긴다
		const bool bStale = !IsValid(Occupant)
			|| (HeistState && !HeistState->IsBoarded(Occupant));

		if (bStale)
		{
			FirstFree = Index;
		}
	}

	// 장부는 덮어쓰지 않는다 — 원래 앉아 있던 사람이 내릴 때 그 자리가 정상적으로 비어야 한다
	if (FirstFree == INDEX_NONE)
	{
		UE_LOG(LogHeist, Warning,
			TEXT("[VanZone:%s] 좌석이 %d개뿐이라 %s 를 마지막 자리에 겹쳐 앉힙니다."),
			*GetName(), Seats.Num(), *Player->GetPlayerName());

		return Seats.Last();
	}

	SeatOccupants[FirstFree] = Player;
	return Seats[FirstFree];
}

void AVanZone::PlaceAtAnchor(ACharacter* Character, const USceneComponent* Anchor)
{
	if (!Character || !Anchor)
	{
		return;
	}

	FVector Location = Anchor->GetComponentLocation();

	// 캡슐 원점은 가운데다. 그대로 넣으면 절반이 바닥에 파묻히고 하차 직후 튀어 오른다
	if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Location.Z += Capsule->GetScaledCapsuleHalfHeight();
	}

	// 스윕하지 않는다. 좌석은 메시 안일 수도 있고, 그때 스윕은 도착을 막아 버린다
	Character->SetActorLocation(Location,
		/*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
}

void AVanZone::ReleaseSeat(const APlayerState* Player)
{
	for (TWeakObjectPtr<APlayerState>& Occupant : SeatOccupants)
	{
		if (Occupant.Get() == Player)
		{
			Occupant.Reset();
			return;
		}
	}
}

void AVanZone::SendBoardedEvent(APawn* Player, int32 NumBoarded) const
{
	// 페이로드 규약은 HHTags::Event_Player_BoardedVan 주석에 있다
	FGameplayEventData Payload;
	Payload.EventTag = HHTags::Event_Player_BoardedVan;
	Payload.Instigator = Player;
	Payload.Target = Player;
	Payload.EventMagnitude = static_cast<float>(NumBoarded);

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Player, HHTags::Event_Player_BoardedVan, Payload);
}

// ──────────────────────────────────────────────────────────────
// 체류 시간
// ──────────────────────────────────────────────────────────────

bool AVanZone::IsLoadAllowedNow() const
{
	// Prep · Heist · Escape 는 전부 받는다. 결과 화면 뒤에 굴러 들어온 것만 막는다 —
	// 이미 표시된 정산액과 GameState 의 값이 달라지기 때문이다.
	// 테스트 맵에는 GameState 가 없다. 그때는 "결과 화면이 떴다" 가 아니므로 막지 않는다
	const AHeistGameState* HeistState = AHeistGameState::Get(this);

	return !HeistState || !HeistState->IsPhase(HHTags::Phase_Result);
}

void AVanZone::TrackPending(ALootBase* Loot)
{
	// 액터 하나가 여러 바디로 겹치면 BeginOverlap 이 여러 번 온다.
	// 그때마다 시작 시각을 새로 찍으면 체류 시간이 영영 안 찬다
	if (PendingLoot.Contains(Loot))
	{
		return;
	}

	// 금액을 두 번 세는 것이 이 시스템에서 낼 수 있는 최악의 결과라 상태로 막는다.
	// 기본 연출에서는 곧 파괴되지만, NPC 연출로 바꾸면 살아남아 존을 드나들 수 있다
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

void AVanZone::RecheckPending()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float DwellSeconds = UHeistSettings::Get()->LoadDwellSeconds;

	// 확정이 PendingLoot 와 오버랩 상태를 건드리므로(파괴 → EndOverlap), 순회 중에 바로
	// 확정하면 반복자 아래에서 컨테이너가 바뀐다. 먼저 고르고 나중에 확정한다
	TArray<ALootBase*> ToConfirm;

	for (auto It = PendingLoot.CreateIterator(); It; ++It)
	{
		ALootBase* Loot = It->Key.Get();

		// 추적 중에 파괴됐다 (파손형). 없어진 물건은 실을 수 없다
		if (!IsValid(Loot))
		{
			It.RemoveCurrent();
			continue;
		}

		// 다시 집어 들었다. 체류 시간은 손에서 떠난 순간부터 다시 센다
		if (IsValid(Loot->GetPrimaryCarrier()))
		{
			It->Value = CarriedMarker;
			continue;
		}

		// 방금 손을 떠났다. 여기서부터가 시작이다
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
		// 체류하는 사이에 페이즈가 넘어갔을 수 있다. 이 시점 기준으로 다시 본다
		if (IsLoadAllowedNow())
		{
			ConfirmLoad(Loot);
		}
	}

	UpdateRecheckTimer();
}

void AVanZone::UpdateRecheckTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();

	// 존 안에 아무것도 없으면 멈춘다. 미션 대부분의 시간이 이 상태다
	if (PendingLoot.IsEmpty())
	{
		Timers.ClearTimer(RecheckTimerHandle);
		return;
	}

	if (!Timers.IsTimerActive(RecheckTimerHandle))
	{
		Timers.SetTimer(RecheckTimerHandle, this, &AVanZone::RecheckPending,
			RecheckIntervalSeconds, /*bLoop=*/true);
	}
}

// ──────────────────────────────────────────────────────────────
// 확정
// ──────────────────────────────────────────────────────────────

void AVanZone::ConfirmLoad(ALootBase* Loot)
{
	if (!HasAuthority() || !IsValid(Loot))
	{
		return;
	}

	// 액터가 사라져도 남아야 하는 것들을 먼저 값으로 뽑는다
	const FHeistLoadEntry Entry = MakeLoadEntry(Loot);

	// 손에서 떠난 뒤에 확정되므로 PrimaryCarrier 는 이미 비어 있다.
	// 던져 넣기까지 포함해 "누가 실었는가" 를 아는 것은 이 값뿐이다
	APawn* Loader = Loot->GetLastCarrier();

	Loot->SetLootStateTag(HHTags::Loot_State_Loaded);

	if (AHeistGameState* HeistState = AHeistGameState::Get(this))
	{
		HeistState->RecordLoadedLoot(Entry);
	}
	else
	{
		UE_LOG(LogHeist, Warning,
			TEXT("[VanZone:%s] AHeistGameState 가 없어 금액을 집계하지 못했다 — %s ($%d)"),
			*GetName(), *Loot->GetName(), Entry.Value);
	}

	SendLoadedEvent(Loader, Loot, Entry.Value);

	UE_LOG(LogHeist, Log, TEXT("적재 확정 — %s ($%d), 적재자 %s"),
		*Loot->GetName(), Entry.Value,
		Loader ? *GetNameSafe(Loader) : TEXT("(알 수 없음)"));

	ShowLoadDebug(FString::Printf(TEXT("적재 확정 — %s ($%d)"), *Loot->GetName(), Entry.Value),
		FColor::Green, Loot->GetActorLocation());

	// 마지막이다. 기본 구현이 노획물을 파괴하므로 이 줄 아래에서 Loot 을 만지면 안 된다
	HandleConfirmedLoot(Loot);
}

FHeistLoadEntry AVanZone::MakeLoadEntry(const ALootBase* Loot) const
{
	FHeistLoadEntry Entry;

	if (!IsValid(Loot))
	{
		return Entry;
	}

	Entry.LootClass = Loot->GetClass();
	Entry.Value = Loot->GetCurrentValue();
	Entry.BaseValue = Loot->GetBaseValue();

	// 특성만 남기고 상태(Loot.State.*)는 뺀다. 결과 화면이 알아야 하는 것은 특성뿐이다
	FGameplayTagContainer Owned;
	Loot->GetOwnedGameplayTags(Owned);
	Entry.TypeTags = Owned.Filter(FGameplayTagContainer(HHTags::Loot_Type.GetTag()));

	// 폰이 아니라 PlayerState 다. 폰은 체포 · 다운으로 파괴되지만 결과 화면은 그 뒤에 뜬다
	if (const APawn* Loader = Loot->GetLastCarrier())
	{
		Entry.Loader = Loader->GetPlayerState();
	}

	return Entry;
}

void AVanZone::SendLoadedEvent(APawn* Loader, ALootBase* Loot, int32 LoadedValue) const
{
	if (!IsValid(Loader))
	{
		// 아무도 든 적 없이 굴러 들어온 경우다. 금액은 이미 들어갔고 기여도만 주인이 없다
		return;
	}

	// 페이로드 규약은 HHTags::Event_Loot_Loaded 주석에 있다. 받는 쪽이 그 약속에 기댄다
	FGameplayEventData Payload;
	Payload.EventTag = HHTags::Event_Loot_Loaded;
	Payload.Instigator = Loot;
	Payload.Target = Loader;
	Payload.OptionalObject = Loot;
	Payload.EventMagnitude = static_cast<float>(LoadedValue);

	// 대상이 IAbilitySystemInterface 가 아니면 이 함수가 조용히 빠진다
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Loader, HHTags::Event_Loot_Loaded, Payload);
}

// ──────────────────────────────────────────────────────────────
// 연출
// ──────────────────────────────────────────────────────────────

void AVanZone::HandleConfirmedLoot_Implementation(ALootBase* Loot)
{
	if (!IsValid(Loot))
	{
		return;
	}

	// 위치를 먼저 뽑는다. 파괴한 뒤에는 물어볼 대상이 없다
	Multicast_PlayLoadedEffect(Loot->GetActorLocation());

	Loot->Destroy();
}

void AVanZone::Multicast_PlayLoadedEffect_Implementation(FVector Location)
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

void AVanZone::SyncBorderDecal()
{
	if (!BorderDecal || !LoadVolume)
	{
		return;
	}

	BorderDecal->SetDecalMaterial(BorderMaterial);
	BorderDecal->SetVisibility(BorderMaterial != nullptr);

	// 액터의 yaw 만 따르는 기준 프레임. 밴이 돌아가면 테두리도 같이 돌지만, 기울지는 않는다
	const FTransform YawFrame(
		FRotator(0.f, GetActorRotation().Yaw, 0.f),
		GetActorLocation(),
		FVector::OneVector);

	// 볼륨의 여덟 모서리를 그 프레임으로 옮겨 정렬된 상자로 다시 잰다.
	// 볼륨이 회전돼 있어도 스케일이 걸려 있어도 여기서 흡수된다
	const FVector VolumeExtent = LoadVolume->GetUnscaledBoxExtent();
	const FTransform VolumeToWorld = LoadVolume->GetComponentTransform();

	FBox Aligned(ForceInit);

	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Offset(
			(Corner & 1) ? VolumeExtent.X : -VolumeExtent.X,
			(Corner & 2) ? VolumeExtent.Y : -VolumeExtent.Y,
			(Corner & 4) ? VolumeExtent.Z : -VolumeExtent.Z);

		Aligned += YawFrame.InverseTransformPosition(VolumeToWorld.TransformPosition(Offset));
	}

	const FVector Extent = Aligned.GetExtent();

	BorderDecal->SetWorldLocation(YawFrame.TransformPosition(Aligned.GetCenter()));

	// Pitch -90 이면 로컬 X 가 아래를 본다. +90 은 위를 봐서 텍스처 V 축이 뒤집힌다
	BorderDecal->SetWorldRotation(FRotator(-90.f, GetActorRotation().Yaw, 0.f));

	// DecalSize 에 컴포넌트 스케일이 곱해진다. 위에서 이미 월드 크기로 쟀으므로 1 로 고정한다
	BorderDecal->SetWorldScale3D(FVector::OneVector);

	// 로컬 X = 아래(투영 깊이), Y = 액터 오른쪽, Z = 액터 앞쪽
	BorderDecal->DecalSize = FVector(Extent.Z + BorderDepthMargin, Extent.Y, Extent.X);

	// 직접 대입이라 렌더 상태가 자동으로 갱신되지 않는다
	BorderDecal->MarkRenderStateDirty();
}

void AVanZone::DrawBorderFallback() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();

	if (!World || !LoadVolume)
	{
		return;
	}

	// 영구 선으로 그리지 않는다. 밴은 진입점으로 옮겨지는데 그 코드는 GameMode 라 서버에만 있고,
	// 클라이언트에는 옮겨진 위치가 BeginPlay 뒤에 복제로 도착한다. 영구 선은 그때 갱신되지 않고
	// 선택적으로 지울 수도 없다(FlushPersistentDebugLines 는 월드 전체를 날린다).
	//
	// 중심을 액터가 아니라 볼륨에서 읽는다 — 루트는 밴 바닥이라 한 칸 아래에 그려진다
	DrawDebugBox(World, LoadVolume->GetComponentLocation(), LoadVolume->GetScaledBoxExtent(),
		LoadVolume->GetComponentQuat(),
		FColor::Yellow, /*bPersistentLines=*/false,
		/*LifeTime=*/BorderDebugLifetimeSeconds, /*DepthPriority=*/0,
		/*Thickness=*/3.f);
#endif
}

void AVanZone::ShowLoadDebug(const FString& Message, const FColor& Color, const FVector& Location) const
{
	if (!bShowLoadDebug)
	{
		return;
	}

	UE_LOG(LogHeist, Log, TEXT("[VanZone:%s] %s"), *GetName(), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, Color,
			FString::Printf(TEXT("[%s] %s"), *GetName(), *Message));
	}

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), Location, 20.f, 12, Color, false, 3.f);
#endif
}
