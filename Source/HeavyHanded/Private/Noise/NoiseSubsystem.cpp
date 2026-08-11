#include "Noise/NoiseSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Shared/NetAuthority.h"
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

	/**
	 * 값싼 1차 거리 컬링에 쓰는 여유 (cm).
	 *
	 * 1차 컬링은 소유 액터 위치로 하고, 정확한 귀 위치는 통과한 청취자에게만 묻는다
	 * (GetListenerLocation 은 BlueprintNativeEvent 라 ProcessEvent 를 탄다).
	 * 그래서 이 값은 "액터 위치와 실제 귀 위치의 최대 차이" 보다 반드시 커야 한다 —
	 * 작게 잡으면 반경 경계에 선 경비가 소리를 못 듣는데, 그건 로그에도 안 남는다.
	 *
	 * 실제 차이는 폰의 시점 높이(BaseEyeHeight 기본 64)와
	 * UPerceptionMeterComponent::EarHeight(기본 60, ClampMax 300)다. 500 이면 충분히 넉넉하다.
	 * EarHeight 의 ClampMax 를 올린다면 이 값도 같이 올릴 것.
	 */
	constexpr float ListenerCullMargin = 500.f;

	/**
	 * 청취자를 소유한 액터. 오클루전 트레이스에서 제외할 대상이다.
	 * 등록 시 한 번만 부른다 — 컴포넌트의 소유 액터는 런타임에 바뀌지 않는다.
	 */
	AActor* ResolveListenerOwner(UObject* ListenerObject)
	{
		if (AActor* AsActor = Cast<AActor>(ListenerObject))
		{
			return AsActor;
		}
		if (const UActorComponent* AsComponent = Cast<UActorComponent>(ListenerObject))
		{
			return AsComponent->GetOwner();
		}
		return nullptr;
	}
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
	OcclusionHitsScratch.Empty();
	WarnedMissingProfiles.Reset();
	CachedProfileTable = nullptr;
	bProfileTableResolved = false;

	Super::Deinitialize();
}

void UNoiseSubsystem::HandleGameStateSet(AGameStateBase* GameState)
{
	if (HasServerAuthority(GetWorld()))
	{
		UAlertComponent::EnsureOnGameState(GameState);
	}
}

TStatId UNoiseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNoiseSubsystem, STATGROUP_Tickables);
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

	// 태그당 한 번만 경고한다.
	// 이 함수는 물리 충돌 경로에서 불리므로, 태그 오타 하나로 구르는 프롭이
	// 초당 수십 줄씩 로그를 쏟는다. Warning 은 Shipping 에서도 컴파일에 남는다
	if (!WarnedMissingProfiles.Contains(Tag))
	{
		WarnedMissingProfiles.Add(Tag);
		UE_LOG(LogNoise, Warning, TEXT("'%s' 에 해당하는 행이 DT_NoiseProfiles 에 없습니다. 소음이 발행되지 않습니다."),
				*Tag.ToString());
	}
	return nullptr;
}

// ──────────────────────────────────────────────────────────────
// 발행
// ──────────────────────────────────────────────────────────────

void UNoiseSubsystem::ReportNoise(FGameplayTag Tag, FVector Location, float LoudnessScale, AActor* Instigator,
								  float AlertScale)
{
	if (!HasServerAuthority(GetWorld()))
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
	Event.AlertScale = FMath::Clamp(AlertScale, 0.f, 1.f);
	// 작은 소리도 반경의 60% 는 퍼진다. 0 이 되면 사실상 안 들리는 것과 같아서
	Event.Radius = Row->Radius * FMath::Lerp(0.6f, 1.0f, Loudness);
	Event.InstigatorActor = Instigator;

	// 감쇄 전 구독자(경계도 · 소음 유발자 집계)에게 먼저 알린다.
	// 청취자 반응이 소음을 재발행하더라도 경계도는 한 번만 오르게 하려는 순서다
	OnNoiseReported.Broadcast(Event, *Row, Instigator);

	Propagate(Event, *Row);
}

FGuid UNoiseSubsystem::StartContinuousNoise(FGameplayTag Tag, AActor* Source)
{
	if (!HasServerAuthority(GetWorld()) || !Source || !Tag.IsValid())
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
	UObject* Object = Listener.GetObject();
	if (!Object)
	{
		return;
	}

	const bool bAlreadyRegistered = Listeners.ContainsByPredicate(
			[Object](const FNoiseListenerEntry& Entry) { return Entry.Listener.Get() == Object; });
	if (bAlreadyRegistered)
	{
		return;
	}

	// 소유 액터는 여기서 한 번만 푼다. Propagate 는 소음 1건마다 전원을 도는 경로다
	FNoiseListenerEntry Entry;
	Entry.Listener   = Object;
	Entry.OwnerActor = ResolveListenerOwner(Object);

	Listeners.Add(MoveTemp(Entry));
}

void UNoiseSubsystem::UnregisterListener(TScriptInterface<INoiseListener> Listener)
{
	if (UObject* Object = Listener.GetObject())
	{
		Listeners.RemoveAllSwap(
				[Object](const FNoiseListenerEntry& Entry) { return Entry.Listener.Get() == Object; });
	}
}

// ──────────────────────────────────────────────────────────────
// 전파 · 감쇄
// ──────────────────────────────────────────────────────────────

void UNoiseSubsystem::Propagate(const FNoiseEvent& Event, const FNoiseProfileRow& Profile)
{
	const bool bGlobal = Profile.bGlobal;

	// Get() 은 CDO 라 null 이 아니다 (NoiseSettings.h 주석 참고).
	// 예전에는 `Settings ? X : 리터럴` 삼항이 있었는데, 죽은 분기인 데다
	// 폴백 리터럴이 UNoiseSettings 의 실제 기본값과 따로 놀아 조용히 어긋날 수 있었다
	const UNoiseSettings& Settings = *UNoiseSettings::Get();
	const float MinAudible = Settings.MinAudibleStrength;

	AActor* InstigatorActor = Event.InstigatorActor.Get();

#if ENABLE_DRAW_DEBUG
	// [디버그 전용] 소음 반경 구체. hh.Noise.Debug 1
	if (CVarNoiseDebug.GetValueOnGameThread() > 0)
	{
		const float DebugSeconds = Settings.DebugDrawDuration;
		DrawDebugSphere(GetWorld(), Event.Location, Event.Radius, 16, FColor::Yellow, false, DebugSeconds);
	}
#endif

	// 죽은 청취자 정리는 순회 전에 한 번에 끝낸다
	Listeners.RemoveAllSwap([](const FNoiseListenerEntry& Entry) { return !Entry.Listener.IsValid(); },
							EAllowShrinking::No);

	// 스냅샷을 떠서 순회한다.
	//
	// OnNoiseHeard 는 게임플레이 콜백이라 무슨 일이든 할 수 있다 — 인지 100% 로 경비가
	// 조사에 들어가고, 그 과정에서 액터가 파괴되면 EndPlay -> UnregisterListener 로
	// Listeners 가 그 자리에서 줄어든다.
	//
	// 예전에는 Listeners 를 역순 인덱스로 직접 돌았는데, Num() 이 루프 시작 시 한 번만
	// 평가되므로 한 콜백에서 둘 이상이 빠지면 인덱스가 배열 밖으로 나간다.
	// TArray::RangeCheck 는 checkf 라 Shipping(DO_CHECK=0)에서 컴파일에서 빠진다 —
	// 개발 빌드는 죽고 Shipping 은 조용히 남의 메모리를 읽는다.
	// RemoveAllSwap 이 뒤 원소를 앞으로 당기는 탓에 같은 청취자가 두 번 불리기도 했다.
	TArray<FNoiseListenerEntry, TInlineAllocator<16>> Snapshot(Listeners.GetData(), Listeners.Num());

	for (const FNoiseListenerEntry& Entry : Snapshot)
	{
		// 스냅샷을 뜬 뒤 이번 순회 도중에 파괴됐을 수 있다.
		// (등록만 해제하고 살아 있는 경우는 이번 1건까지는 받는다 — 청취자 쪽 게이트가 걸러낸다)
		UObject* ListenerObject = Entry.Listener.Get();
		if (!ListenerObject)
		{
			continue;
		}

		// 트레이스에서 제외할 액터. 등록 때 풀어둔 것을 그대로 쓴다
		const AActor* ListenerActor = Entry.OwnerActor.Get();

		float Strength = Event.Loudness01;

		if (!bGlobal)
		{
			// 1차 컬링은 액터 위치로 값싸게 한다.
			// 정확한 귀 위치를 묻는 GetListenerLocation 은 BlueprintNativeEvent 라 ProcessEvent 를 타므로,
			// 반경 근처에도 없는 청취자에게까지 리플렉션 호출을 낼 이유가 없다.
			// ListenerCullMargin 이 액터 위치와 귀 위치의 차이를 덮는다 (상단 주석 참고)
			if (ListenerActor)
			{
				const float CoarseRange = Event.Radius + ListenerCullMargin;
				if (FVector::DistSquared(Event.Location, ListenerActor->GetActorLocation())
						> CoarseRange * CoarseRange)
				{
					continue;
				}
			}

			const FVector ListenerLocation = INoiseListener::Execute_GetListenerLocation(ListenerObject);

#if !UE_BUILD_SHIPPING
			// 1차 컬링의 전제를 개발 빌드에서 시끄럽게 검증한다.
			// GetListenerLocation 은 BlueprintNativeEvent 라 BP 가 임의 지점을 돌려줄 수 있는데,
			// 그게 액터에서 ListenerCullMargin 을 넘어가면 반경 경계의 청취자가 조용히 컬링된다.
			// 통과한 청취자만 볼 수 있지만, 계약 위반은 대개 여기서 먼저 드러난다
			if (ListenerActor)
			{
				const float EarOffset = FVector::Dist(ListenerLocation, ListenerActor->GetActorLocation());
				ensureMsgf(EarOffset <= ListenerCullMargin,
						TEXT("%s 의 GetListenerLocation 이 소유 액터에서 %.0fcm 떨어져 있습니다 (허용 %.0f). ")
						TEXT("1차 거리 컬링이 이 청취자를 잘못 걸러낼 수 있습니다."),
						*GetNameSafe(ListenerActor), EarOffset, ListenerCullMargin);
			}
#endif

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
		Stimulus.Grade           = Profile.Grade;
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

	const UNoiseSettings& Settings = *UNoiseSettings::Get();
	const float Exponent     = Settings.DistanceFalloffExponent;
	const int32 MaxOccluders = Settings.MaxOccluders;

	float Attenuation = 1.f - FMath::Pow(Distance / Radius, Exponent);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NoiseOcclusion), /*bTraceComplex*/ false);
	Params.bReturnPhysicalMaterial = true;
	if (IgnoreA) { Params.AddIgnoredActor(IgnoreA); }
	if (IgnoreB) { Params.AddIgnoredActor(IgnoreB); }

	// 스크래치 버퍼를 재사용한다. 지역 TArray 로 두면 트레이스마다 힙 할당이 일어나는데
	// (FHitResult 는 100바이트가 넘는다) 이 함수는 (소음 1건 × 반경 안 청취자 수) 만큼 호출된다.
	// LineTraceMultiByChannel 이 내부에서 Reset 하므로 우리가 비울 필요는 없다.
	// 엔진 시그니처가 기본 얼로케이터를 요구해서 TInlineAllocator 는 못 쓴다.
	// 게임 스레드 전용이다 — 다른 스레드에서 부르면 이 버퍼가 깨진다
	World->LineTraceMultiByChannel(OcclusionHitsScratch, From, To, NoiseOcclusionChannel, Params);

	// 히트는 거리순으로 정렬돼 있다 (CollisionConversions.cpp - ConvertTraceResults 끝의
	// OutHits.Sort(FCompareFHitResultTime)). 그래서 앞에서부터 MaxOccluders 개만 세는 것이
	// "가장 가까운 차폐물 N개" 와 같다.
	//
	// 전제: NoiseOcclusion 채널은 Overlap 응답이어야 한다 (DefaultEngine.ini).
	// Block 으로 바꾸는 순간 LineTraceMulti 가 첫 벽에서 광선을 끊고 그보다 먼 히트를
	// 전부 버리므로(Chaos SQTypes.h - FinishQueryHelper) 이 루프는 영원히 1 회만 돈다.
	// 컴파일 에러도 로그도 없이 MaxOccluders 세팅만 조용히 죽는다
	int32 Counted = 0;
	for (const FHitResult& Hit : OcclusionHitsScratch)
	{
		if (Counted >= MaxOccluders)
		{
			break;
		}

		const EPhysicalSurface Surface = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
		Attenuation *= Settings.GetOccluderFactor(Surface);
		++Counted;
	}

#if ENABLE_DRAW_DEBUG
	// [디버그 전용] 감쇄 트레이스. 빨강 = 차폐물에 막힘, 초록 = 직선 도달. hh.Noise.Debug 2
	if (CVarNoiseDebug.GetValueOnGameThread() > 1)
	{
		const float DebugSeconds = Settings.DebugDrawDuration;
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

	if (ContinuousNoises.IsEmpty() || !HasServerAuthority(GetWorld()))
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
		// 음수를 누적하지 않는다. 그냥 += 하면 2초짜리 히치 뒤에 -1.75 가 남아
		// 회복될 때까지 여러 프레임 연속으로 발행되면서 경계도가 한 번에 튄다
		Noise.TimeUntilNextEmit = FMath::Max(Noise.TimeUntilNextEmit + ContinuousEmitInterval, 0.f);

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
//
// 쉬핑 빌드에서는 코드째 빠진다 — 위 CVarNoiseDebug 와 같은 규칙이다.
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

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
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NoiseTestCommand),
	  ECVF_Cheat);

#endif   // !UE_BUILD_SHIPPING
