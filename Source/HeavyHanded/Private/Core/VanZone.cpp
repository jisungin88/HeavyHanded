#include "Core/VanZone.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "UObject/ConstructorHelpers.h"
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

	/**
	 * 좌석 수. 기획서 2~4인이라 넷이면 충분하다.
	 *
	 * 모자랄 때 승차를 거부하지 않고 마지막 좌석에 겹쳐 앉히므로, 이 값이 틀려도
	 * 누가 못 타는 일은 없다. 파티 인원이 늘면 여기 하나만 고친다.
	 */
	constexpr int32 MaxSeats = 4;

	/**
	 * PendingLoot 의 값이 이것이면 아직 누가 들고 있다는 뜻이다.
	 *
	 * 별도 bool 을 두지 않는 이유는 두 값이 어긋날 수 없게 하기 위해서다 —
	 * "들려 있는데 체류 시각이 찍혀 있는" 상태가 아예 표현되지 않는다.
	 */
	constexpr float CarriedMarker = -1.f;
}

AVanZone::AVanZone()
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

	BoardVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardVolume"));
	BoardVolume->SetupAttachment(LoadVolume);

	BoardVolume->SetBoxExtent(FVector(DefaultZoneExtent));
	BoardVolume->SetCollisionProfileName(VanBoardZoneProfile);
	BoardVolume->SetHiddenInGame(true);

	// 콜백은 걸지 않지만 겹침 추적은 켜야 한다. IsOverlappingActor 가 이 목록을 읽는다
	BoardVolume->SetGenerateOverlapEvents(true);

	VanDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VanDoor"));
	VanDoor->SetupAttachment(LoadVolume);

	// 그레이박스용 기본값. 큐브를 얇게 눌러 문짝 모양으로 쓴다.
	// 비워 두면 조준할 것이 없어 승차가 아예 불가능해지므로 기본값이 있어야 한다
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultDoorMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultDoorMesh.Succeeded())
	{
		VanDoor->SetStaticMesh(DefaultDoorMesh.Object);
	}

	// 볼륨 뒤쪽 면에 세워 둔다. 두께 10cm · 폭 140cm · 높이 200cm 짜리 문짝이다
	VanDoor->SetRelativeLocation(FVector(DefaultZoneExtent, 0.f, 0.f));
	VanDoor->SetRelativeScale3D(FVector(0.1f, 1.4f, 2.f));

	// 조준되는 것 하나가 이 컴포넌트의 전부다. 헤더 주석 참고 —
	// 몸체가 아니라 문이므로 막을 것도 가릴 것도 없다
	VanDoor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VanDoor->SetCollisionResponseToAllChannels(ECR_Ignore);
	VanDoor->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 좌석. 화물칸 바닥에 둘씩 마주 보게 흩어 두고, 실제 배치는 뷰포트에서 한다.
	// Z 가 볼륨 아래 면인 이유는 앵커가 '발이 닿는 지점' 이기 때문이다
	static const FVector DefaultSeatOffsets[] = {
		FVector(-60.f, -60.f, -DefaultZoneExtent), FVector(-60.f,  60.f, -DefaultZoneExtent),
		FVector( 30.f, -60.f, -DefaultZoneExtent), FVector( 30.f,  60.f, -DefaultZoneExtent)
	};

	Seats.Reserve(MaxSeats);
	for (int32 Index = 0; Index < MaxSeats; ++Index)
	{
		USceneComponent* Seat = CreateDefaultSubobject<USceneComponent>(
			*FString::Printf(TEXT("Seat%d"), Index));
		Seat->SetupAttachment(LoadVolume);
		Seat->SetRelativeLocation(DefaultSeatOffsets[Index]);

		Seats.Add(Seat);
	}

	// 하차 지점. 기본값은 문짝 바깥쪽 바닥이다 — 문을 열고 뒤로 내리는 그림
	ExitAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ExitAnchor"));
	ExitAnchor->SetupAttachment(LoadVolume);
	ExitAnchor->SetRelativeLocation(FVector(DefaultZoneExtent + 80.f, 0.f, -DefaultZoneExtent));

	BorderDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("BorderDecal"));
	BorderDecal->SetupAttachment(LoadVolume);

	// 데칼은 자기 로컬 X 축 방향으로 투영한다. 바닥에 그리려면 그 축이 아래를 봐야 한다.
	BorderDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

	// 크기는 볼륨에서 따라간다. 머티리얼이 없으면 OnConstruction 이 꺼 버린다.
	BorderDecal->SetVisibility(false);
}

void AVanZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 볼륨을 늘리는 즉시 테두리도 같이 늘어나야 한다.
	// 배치 담당이 크기를 맞춰 놓고 게임을 켰더니 테두리만 옛 크기인 상황을 만들지 않는다.
	SyncBorderDecal();
}

void AVanZone::BeginPlay()
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

	LoadVolume->OnComponentBeginOverlap.AddDynamic(this, &AVanZone::HandleBeginOverlap);
	LoadVolume->OnComponentEndOverlap.AddDynamic(this, &AVanZone::HandleEndOverlap);

	// BoardVolume 에는 콜백을 걸지 않는다. 승차는 드나드는 것이 아니라 상호작용이다

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

void AVanZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환 중에 재검사가 돌면 이미 정리되기 시작한 월드를 건드린다.
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
// 오버랩 — 존 안에 들어왔는가
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

	// 확정 조건은 여기서 보지 않는다. 들려 있든 아니든 일단 추적을 시작하고,
	// 손을 떠난 시점부터 체류 시간을 재는 것은 재검사 쪽 일이다.
	TrackPending(Loot);
}

void AVanZone::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
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
// 승차 — 상태이지 사건이 아니다
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

	// 레벨당 하나라는 계약에 기대어 첫 번째를 돌려준다.
	// 둘 이상이면 BeginPlay 가 이미 경고를 남겼으므로 여기서 다시 따지지 않는다.
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

	// 명단이 진리원이다. 밴을 먼저 찾아 물어보지 않는 이유는, 타고 있지 않은 경우가
	// 압도적으로 흔한데 그때마다 액터를 훑을 이유가 없기 때문이다
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
	// 상호작용 판정은 서버에서만 돈다 (UGAB_Interact 도 서버에서만 부른다).
	// 클라이언트가 자기 명단을 고칠 수 있으면 탈출 판정이 곧 치트가 된다
	if (!HasAuthority() || !IsValid(Player))
	{
		return false;
	}

	// 조종자가 없는 폰은 명단에 오를 수 없다. 그것을 사람으로 세면 인원이 맞지 않는다
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

	// 결과 화면이 뜬 뒤에는 타지도 내리지도 못한다.
	//
	// 명단은 이미 얼어 있으므로(AHeistGameState::SetBoarded) 여기를 안 막아도 집계는
	// 안전하다. 그런데도 막는 것은 몸 때문이다 — 명단에는 탈출로 남은 사람이 밴에서
	// 걸어 나오면 결과 화면과 눈앞의 장면이 어긋난다.
	if (HeistState->IsPhase(HHTags::Phase_Result))
	{
		ShowLoadDebug(TEXT("거부 — 결과 화면에서는 타고 내릴 수 없다"),
			FColor::Red, Player->GetActorLocation());
		return false;
	}

	const bool bWasBoarded = HeistState->IsBoarded(PlayerState);

	// 내리는 것은 (결과 화면을 빼면) 언제나 된다. 탈 때만 조건을 본다 —
	// 페이즈가 넘어갔다고 탄 사람을 밴에 가둬 둘 이유가 없다
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

	// 이벤트는 "일어난 일" 이라 탈 때만 보낸다.
	// 내리는 것은 별도 이벤트가 아니라 State.InVan 이 떨어지는 것으로 표현된다
	if (bBoarded)
	{
		SendBoardedEvent(Player, HeistState->GetBoardedNum());
	}

	return true;
}

bool AVanZone::CanBoardNow(const APawn* Player) const
{
	// 뒷칸 밖에서 멀리 조준해 타는 것을 막는다. 어빌리티의 사거리(기본 300cm)만으로는
	// 밴 옆이나 지붕 위에서도 닿기 때문에, "안에 들어와 있는가" 를 여기서 다시 본다
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

	// 준비 시간에 타 봐야 할 일이 없고, 그때 이동이 묶이면 준비 시간을 통째로 날린다.
	// 결과 화면이 뜬 뒤도 마찬가지다 — 이미 끝난 판이다
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
			// 자리로 옮긴 뒤 붙인다. 회전은 건드리지 않는다 — 시점 회전은 컨트롤러가
			// 매 프레임 덮어쓰므로, 여기서 돌려 놔도 다음 프레임에 되돌아간다
			PlaceAtAnchor(Character, Seat);

			Character->AttachToComponent(Seat, FAttachmentTransformRules::KeepWorldTransform);
		}
		else
		{
			// 좌석이 하나도 없다. 서 있던 자리에 고정한다 — 자리가 없다고 못 타게 하면
			// 그 사람은 영영 탈출하지 못한다
			Character->AttachToComponent(GetRootComponent(),
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

		// 떼어낸 뒤에 옮긴다. 붙어 있는 채로 위치를 바꾸면 어태치가 그 값을 상대 좌표로
		// 다시 해석해서 엉뚱한 곳으로 간다
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

	// 좌석 수가 바뀌어도(파티 인원 조정) 장부가 따라오게 매번 맞춘다
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

		// 비었는가를 '상태' 가 아니라 '사실' 로 본다. 접속이 끊긴 사람의 자리가 영영 잠기면
		// 나중에 들어온 사람이 앉을 곳이 없어지는데, 그건 하차 처리를 놓치는 순간 바로 생긴다
		const bool bStale = !IsValid(Occupant)
			|| (HeistState && !HeistState->IsBoarded(Occupant));

		if (bStale)
		{
			FirstFree = Index;
		}
	}

	// 자리가 없으면 마지막 좌석에 겹쳐 앉힌다. 장부는 덮어쓰지 않는다 —
	// 원래 앉아 있던 사람이 내릴 때 그 자리가 정상적으로 비어야 한다
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

	// 캡슐 원점은 가운데다. 앵커 좌표를 그대로 넣으면 절반이 바닥에 파묻히고,
	// 하차 직후 물리가 캐릭터를 위로 밀어내면서 튀어 오른다
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
// 체류 시간 — 놓기와 던지기는 오버랩 이벤트를 만들지 않는다
// ──────────────────────────────────────────────────────────────

bool AVanZone::IsLoadAllowedNow() const
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

void AVanZone::TrackPending(ALootBase* Loot)
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

void AVanZone::RecheckPending()
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

void AVanZone::UpdateRecheckTimer()
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
			TEXT("[VanZone:%s] AHeistGameState 가 없어 금액을 집계하지 못했다 — %s ($%d)"),
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

void AVanZone::SendLoadedEvent(APawn* Loader, ALootBase* Loot, int32 LoadedValue) const
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

void AVanZone::HandleConfirmedLoot_Implementation(ALootBase* Loot)
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

	// 스케일은 컴포넌트가 부모에게서 함께 받으므로 여기서는 스케일 전 크기로 맞춘다.
	//
	// 데칼의 로컬 X 가 투영 방향(아래), Y 가 월드 Y, Z 가 월드 X 다 — 위 회전(Pitch -90) 때문이다.
	// X 에 볼륨 높이를 그대로 주면 존의 위아래를 전부 덮어, 밴 바닥이 어디에 있든 그려진다.
	const FVector Extent = LoadVolume->GetUnscaledBoxExtent();
	BorderDecal->DecalSize = FVector(Extent.Z, Extent.Y, Extent.X);
}

void AVanZone::DrawBorderFallback() const
{
	UE_LOG(LogHeist, Warning,
		TEXT("[VanZone:%s] BorderMaterial 이 없어 존 테두리를 디버그 선으로 그립니다. "
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
