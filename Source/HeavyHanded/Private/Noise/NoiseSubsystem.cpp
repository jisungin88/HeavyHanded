#include "Noise/NoiseSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Noise/NoiseListener.h"
#include "Noise/NoiseSettings.h"

#include "GameFramework/GameStateBase.h"
#include "Alert/AlertComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoise, Log, All);

namespace
{
	/** Config/DefaultEngine.ini 의 NoiseOcclusion. 채널 번호가 바뀌면 여기만 고친다 */
	constexpr ECollisionChannel NoiseOcclusionChannel = ECC_GameTraceChannel1;

	/** 지속형 소음 발행 주기. 기획서의 "+3%/초" 를 4Hz 로 쪼갠다 */
	constexpr float ContinuousEmitInterval = 0.25f;
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 시각화 스위치
//
// ECVF_Cheat 라 쉬핑 빌드에서는 콘솔로 켤 수 없고,
// ENABLE_DRAW_DEBUG 가 0이면 그리기 코드 자체가 컴파일에서 빠진다.
// 게임 로직이 이 값을 읽어서는 안 된다 — 빌드 구성에 따라 동작이 달라진다.
// ──────────────────────────────────────────────────────────────
static TAutoConsoleVariable<int32> CVarNoiseDebug(
	  TEXT("hh.Noise.Debug"),
	  0,
	  TEXT("0 = 끔, 1 = 소음 반경 구체, 2 = + 오클루전 트레이스 라인"),
	  ECVF_Cheat);

// ──────────────────────────────────────────────────────────────
// 수명 주기
// ──────────────────────────────────────────────────────────────

UNoiseSubsystem* UNoiseSubsystem::Get(const UObject* WorldContext)
{
	if (!GEngine)
	{
		return nullptr;
	}
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UNoiseSubsystem>();
	}
	return nullptr;
}

void UNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);   // ← 빼면 bInitialized 가 안 켜져서 Tick 이 영원히 안 돈다

	Listeners.Reset();
	ContinuousNoises.Reset();
	
	// 경계도 컴포넌트를 GameState 에 자동 부착한다.
	// GameMode/GameState 클래스를 건드리지 않으려는 것 — 머지 충돌 회피
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* ExistingGameState = World->GetGameState())
		{
			HandleGameStateSet(ExistingGameState);   // 서브시스템이 늦게 만들어진 경우
		}

		GameStateSetHandle =
				World->GameStateSetEvent.AddUObject(this, &UNoiseSubsystem::HandleGameStateSet);
	}
}

void UNoiseSubsystem::Deinitialize()
{
	if (GameStateSetHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GameStateSetEvent.Remove(GameStateSetHandle);
		}
		GameStateSetHandle.Reset();
	}

	Listeners.Reset();
	ContinuousNoises.Reset();
	CachedProfileTable = nullptr;
	bProfileTableResolved = false;

	Super::Deinitialize();
}

void UNoiseSubsystem::HandleGameStateSet(AGameStateBase* GameState)
{
	if (HasNoiseAuthority())
	{
		UAlertComponent::EnsureOnGameState(GameState);
	}
}

TStatId UNoiseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNoiseSubsystem, STATGROUP_Tickables);
}

bool UNoiseSubsystem::HasNoiseAuthority() const
{
	const UWorld* World = GetWorld();
	return World && !World->IsNetMode(NM_Client);
}

// ──────────────────────────────────────────────────────────────
// 프로파일 조회
// ──────────────────────────────────────────────────────────────

UDataTable* UNoiseSubsystem::GetProfileTable()
{
	if (!bProfileTableResolved)
	{
		bProfileTableResolved = true;

		if (const UNoiseSettings* Settings = UNoiseSettings::Get())
		{
			CachedProfileTable = Settings->NoiseProfiles.LoadSynchronous();
		}

		UE_CLOG(!CachedProfileTable, LogNoise, Warning,
				TEXT("NoiseProfiles DataTable 이 지정되지 않았습니다. "
					 "Project Settings → Game → Noise 에서 DT_NoiseProfiles 를 지정하세요."));
	}
	return CachedProfileTable;
}

const FNoiseProfileRow* UNoiseSubsystem::FindProfile(FGameplayTag Tag)
{
	UDataTable* Table = GetProfileTable();
	if (!Table || !Tag.IsValid())
	{
		return nullptr;
	}

	static const TCHAR* Context = TEXT("UNoiseSubsystem::FindProfile");

	if (const FNoiseProfileRow* Row = Table->FindRow<FNoiseProfileRow>(Tag.GetTagName(), Context, /*bWarnIfMissing*/ false))
	{
		return Row;
	}

	// 정확히 일치하는 행이 없으면 부모 태그로 거슬러 올라간다
	for (FGameplayTag Parent = Tag.RequestDirectParent(); Parent.IsValid(); Parent = Parent.RequestDirectParent())
	{
		if (const FNoiseProfileRow* Row = Table->FindRow<FNoiseProfileRow>(Parent.GetTagName(), Context, false))
		{
			UE_LOG(LogNoise, Verbose, TEXT("'%s' 행이 없어 부모 '%s' 로 폴백했습니다."),
					*Tag.ToString(), *Parent.ToString());
			return Row;
		}
	}

	UE_LOG(LogNoise, Warning, TEXT("'%s' 에 해당하는 행이 DT_NoiseProfiles 에 없습니다. 소음이 발행되지 않습니다."),
			*Tag.ToString());
	return nullptr;
}

// ──────────────────────────────────────────────────────────────
// 발행
// ──────────────────────────────────────────────────────────────

void UNoiseSubsystem::ReportNoise(FGameplayTag Tag, FVector Location, float LoudnessScale, AActor* Instigator)
{
	if (!HasNoiseAuthority())
	{
		return;   // 클라의 물리 충돌은 전부 무시한다
	}

	const FNoiseProfileRow* Row = FindProfile(Tag);
	if (!Row)
	{
		return;
	}

	const float Loudness = FMath::Clamp(LoudnessScale, 0.f, 1.f);
	if (Loudness <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FNoiseEvent Event;
	Event.Tag = Tag;
	Event.Location = Location;
	Event.Loudness01 = Loudness;
	// 작은 소리도 반경의 60% 는 퍼진다. 0 이 되면 사실상 안 들리는 것과 같아서
	Event.Radius = Row->Radius * FMath::Lerp(0.6f, 1.0f, Loudness);
	Event.InstigatorActor = Instigator;

	// 감쇄 전 구독자(경계도 · 소음 유발자 집계)에게 먼저 알린다.
	// 청취자 반응이 소음을 재발행하더라도 경계도는 한 번만 오르게 하려는 순서다
	OnNoiseReported.Broadcast(Event, *Row, Instigator);

	Propagate(Event, Row->Grade, Row->bGlobal);
}

FGuid UNoiseSubsystem::StartContinuousNoise(FGameplayTag Tag, AActor* Source)
{
	if (!HasNoiseAuthority() || !Source || !Tag.IsValid())
	{
		return FGuid();
	}

	FContinuousNoise Noise;
	Noise.Tag = Tag;
	Noise.Source = Source;
	Noise.TimeUntilNextEmit = 0.f;   // 첫 발행은 즉시

	const FGuid Handle = FGuid::NewGuid();
	ContinuousNoises.Add(Handle, Noise);
	return Handle;
}

void UNoiseSubsystem::StopContinuousNoise(FGuid Handle)
{
	ContinuousNoises.Remove(Handle);
}

// ──────────────────────────────────────────────────────────────
// 청취자
// ──────────────────────────────────────────────────────────────

void UNoiseSubsystem::RegisterListener(TScriptInterface<INoiseListener> Listener)
{
	if (UObject* Object = Listener.GetObject())
	{
		Listeners.AddUnique(Object);
	}
}

void UNoiseSubsystem::UnregisterListener(TScriptInterface<INoiseListener> Listener)
{
	if (UObject* Object = Listener.GetObject())
	{
		Listeners.RemoveAllSwap([Object](const TWeakObjectPtr<UObject>& Weak) { return Weak.Get() == Object; });
	}
}

// ──────────────────────────────────────────────────────────────
// 전파 · 감쇄
// ──────────────────────────────────────────────────────────────

void UNoiseSubsystem::Propagate(const FNoiseEvent& Event, ENoiseGrade Grade, bool bGlobal)
{
	const UNoiseSettings* Settings = UNoiseSettings::Get();
	const float MinAudible   = Settings ? Settings->MinAudibleStrength : 0.05f;

	AActor* InstigatorActor = Event.InstigatorActor.Get();

#if ENABLE_DRAW_DEBUG
	// [디버그 전용] 소음 반경 구체. hh.Noise.Debug 1
	if (CVarNoiseDebug.GetValueOnGameThread() > 0)
	{
		const float DebugSeconds = Settings ? Settings->DebugDrawDuration : 2.f;
		DrawDebugSphere(GetWorld(), Event.Location, Event.Radius, 16, FColor::Yellow, false, DebugSeconds);
	}
#endif

	// 역순 순회 — 죽은 청취자를 순회 중에 제거해도 인덱스가 안 깨진다
	for (int32 Index = Listeners.Num() - 1; Index >= 0; --Index)
	{
		UObject* ListenerObject = Listeners[Index].Get();
		if (!ListenerObject)
		{
			Listeners.RemoveAtSwap(Index);
			continue;
		}

		float Strength = Event.Loudness01;

		if (!bGlobal)
		{
			const FVector ListenerLocation = INoiseListener::Execute_GetListenerLocation(ListenerObject);

			// 청취자를 소유한 액터는 트레이스에서 제외한다 (자기 콜리전에 막히면 안 되므로)
			const AActor* ListenerActor = Cast<AActor>(ListenerObject);
			if (!ListenerActor)
			{
				if (const UActorComponent* Component = Cast<UActorComponent>(ListenerObject))
				{
					ListenerActor = Component->GetOwner();
				}
			}

			Strength *= ComputeAttenuation(Event.Location, ListenerLocation, Event.Radius,
										   InstigatorActor, ListenerActor);
		}
		if (Strength < MinAudible)
		{
			continue;
		}

		FNoiseStimulus Stimulus;
		Stimulus.Tag             = Event.Tag;
		Stimulus.Location        = Event.Location;
		Stimulus.Strength        = FMath::Min(Strength, 1.f);
		Stimulus.Grade           = Grade;
		Stimulus.InstigatorActor = InstigatorActor;

		INoiseListener::Execute_OnNoiseHeard(ListenerObject, Stimulus);
	}
}

float UNoiseSubsystem::ComputeAttenuation(const FVector& From, const FVector& To, float Radius,
										  const AActor* IgnoreA, const AActor* IgnoreB) const
{
	const UWorld* World = GetWorld();
	if (!World || Radius <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float Distance = FVector::Dist(From, To);
	if (Distance > Radius)
	{
		return 0.f;   // 반경 밖이면 트레이스조차 하지 않는다. 비용이 여기서 결정된다
	}

	const UNoiseSettings* Settings = UNoiseSettings::Get();
	const float Exponent     = Settings ? Settings->DistanceFalloffExponent : 1.7f;
	const int32 MaxOccluders = Settings ? Settings->MaxOccluders            : 4;

	float Attenuation = 1.f - FMath::Pow(Distance / Radius, Exponent);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoiseOcclusion), /*bTraceComplex*/ false);
	Params.bReturnPhysicalMaterial = true;
	if (IgnoreA) { Params.AddIgnoredActor(IgnoreA); }
	if (IgnoreB) { Params.AddIgnoredActor(IgnoreB); }

	TArray<FHitResult> Hits;
	World->LineTraceMultiByChannel(Hits, From, To, NoiseOcclusionChannel, Params);

	int32 Counted = 0;
	for (const FHitResult& Hit : Hits)
	{
		if (Counted >= MaxOccluders)
		{
			break;
		}

		const EPhysicalSurface Surface = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
		Attenuation *= Settings ? Settings->GetOccluderFactor(Surface) : 0.5f;
		++Counted;
	}

#if ENABLE_DRAW_DEBUG
	// [디버그 전용] 감쇄 트레이스. 빨강 = 차폐물에 막힘, 초록 = 직선 도달. hh.Noise.Debug 2
	if (CVarNoiseDebug.GetValueOnGameThread() > 1)
	{
		const float DebugSeconds = Settings ? Settings->DebugDrawDuration : 2.f;
		DrawDebugLine(World, From, To, Counted > 0 ? FColor::Red : FColor::Green, false, DebugSeconds, 0, 1.f);
	}
#endif

	return FMath::Max(Attenuation, 0.f);
}

// ──────────────────────────────────────────────────────────────
// 지속형 소음 티커
// ──────────────────────────────────────────────────────────────

void UNoiseSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ContinuousNoises.IsEmpty() || !HasNoiseAuthority())
	{
		return;
	}

	struct FPendingEmit
	{
		FGameplayTag Tag;
		FVector      Location;
		AActor*      Source;
	};

	TArray<FPendingEmit, TInlineAllocator<4>> Pending;
	TArray<FGuid,        TInlineAllocator<4>> Expired;

	for (TPair<FGuid, FContinuousNoise>& Pair : ContinuousNoises)
	{
		FContinuousNoise& Noise = Pair.Value;

		AActor* Source = Noise.Source.Get();
		if (!Source)
		{
			Expired.Add(Pair.Key);   // 소음원이 파괴되면 자동 종료
			continue;
		}

		Noise.TimeUntilNextEmit -= DeltaTime;
		if (Noise.TimeUntilNextEmit > 0.f)
		{
			continue;
		}
		Noise.TimeUntilNextEmit += ContinuousEmitInterval;

		Pending.Add({ Noise.Tag, Source->GetActorLocation(), Source });
	}

	for (const FGuid& Handle : Expired)
	{
		ContinuousNoises.Remove(Handle);
	}

	// 맵 순회가 끝난 뒤에 발행한다 — 청취자가 반응하다가 Start/Stop 을 부르면
	// 순회 중이던 TMap 이 재할당되어 참조가 날아간다
	for (const FPendingEmit& Emit : Pending)
	{
		ReportNoise(Emit.Tag, Emit.Location, 1.f, Emit.Source);
	}
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 콘솔 명령
//
// DT_NoiseProfiles 없이도 발행 → 조회 → 감쇄 경로를 검증하려고 만든 것이다.
// 게임 코드에서 호출하지 말 것. 7단계 NoiseEmitterComponent 가 붙으면
// 실제 소음은 전부 물리 충돌에서 나온다.
// ──────────────────────────────────────────────────────────────
static void NoiseTestCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	UNoiseSubsystem* Subsystem = World->GetSubsystem<UNoiseSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogNoise, Warning, TEXT("NoiseSubsystem 이 없습니다."));
		return;
	}

	const FString TagString = Args.IsValidIndex(0) ? Args[0] : TEXT("Noise.Loot.Impact");
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), /*ErrorIfNotFound*/ false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogNoise, Warning, TEXT("알 수 없는 태그: %s"), *TagString);
		return;
	}

	const float Loudness = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 1.f;

	FVector Location = FVector::ZeroVector;
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			Location = Pawn->GetActorLocation();
		}
	}

	Subsystem->ReportNoise(Tag, Location, Loudness, nullptr);
	UE_LOG(LogNoise, Log, TEXT("테스트 발행: %s @ %s (Loudness %.2f)"), *Tag.ToString(), *Location.ToString(), Loudness);
}

static FAutoConsoleCommandWithWorldAndArgs GNoiseTestCommand(
	  TEXT("hh.Noise.Test"),
	  TEXT("hh.Noise.Test <Tag> [Loudness] — 플레이어 위치에 소음을 발행한다. 예: hh.Noise.Test Noise.Loot.Throw 1.0"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NoiseTestCommand));
