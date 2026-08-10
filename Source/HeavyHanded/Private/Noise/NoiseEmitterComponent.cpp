#include "Noise/NoiseEmitterComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"

#include "Shared/NetAuthority.h"
#include "Noise/NoiseSettings.h"
#include "Noise/NoiseSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoiseEmitter, Log, All);

namespace
{
	/** 프로파일 행을 못 찾았을 때의 쿨다운. FNoiseProfileRow::CooldownSeconds 기본값과 맞춘다 */
	constexpr float FallbackCooldownSeconds = 0.25f;

	/**
	 * 프로파일에 커브가 없으면 선형으로 둔다.
	 * 커브는 BeginPlay 에서 미리 해석해 둔 것을 받는다 — 히트 경로에서 소프트 참조를
	 * 처음 만지면 물리 콜백 안에서 동기 로드가 걸린다
	 */
	float ApplyImpactCurve(const UCurveFloat* Curve, float Base01)
	{
		if (Curve)
		{
			return FMath::Clamp(Curve->GetFloatValue(Base01), 0.f, 1.f);
		}
		return Base01;
	}

	/**
	 * 프리미티브의 물리 표면을 얻는다.
	 *
	 * 각 컴포넌트는 반드시 "자기" 바디에서 읽는다. 예전에는 FHitResult::PhysMaterial 을 먼저 봤는데,
	 * 두 호출에 같은 Hit 을 넘기다 보니 그 값이 유효한 순간 두 컴포넌트가 같은 표면을 돌려주고
	 * "두 재질 중 작은 쪽이 이긴다" 규칙이 아무 로그 없이 사라졌다.
	 *
	 * PreferredMaterial 은 "맞은 지점" 의 재질이라 복합 재질 메시에서는 바디 기본값보다 정확하다.
	 * 그래서 폴백이 아니라 우선값이다. 다만 한쪽에만 줘야 한다.
	 *
	 * 시뮬레이션 히트 이벤트의 FHitResult 는 PhysMaterial 이 비어 있는 경우가 많다
	 * (bReturnPhysicalMaterial 은 트레이스 쿼리 옵션이다). 그때 바디 인스턴스로 떨어진다.
	 */
	EPhysicalSurface ResolveSurface(const UPrimitiveComponent* Component, const UPhysicalMaterial* PreferredMaterial)
	{
		if (PreferredMaterial)
		{
			return UPhysicalMaterial::DetermineSurfaceType(PreferredMaterial);
		}

		if (Component)
		{
			if (const FBodyInstance* Body = Component->GetBodyInstance())
			{
				if (UPhysicalMaterial* Material = Body->GetSimplePhysicalMaterial())
				{
					return UPhysicalMaterial::DetermineSurfaceType(Material);
				}
			}
		}

		return SurfaceType_Default;
	}
}

// ──────────────────────────────────────────────────────────────
// 수명 주기
// ──────────────────────────────────────────────────────────────

UNoiseEmitterComponent::UNoiseEmitterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 쿨다운이 돌고 있을 때만 틱한다. 노획물이 맵에 수십 개 깔리므로 기본은 꺼둔다
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UNoiseEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasServerAuthority(this))
	{
		return;   // 클라의 물리 충돌은 신뢰하지 않는다. 델리게이트를 아예 안 건다
	}

	if (!ImpactTag.IsValid())
	{
		// 에디터에서 안 채웠으면 기본 태그로 폴백한다
		ImpactTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Loot.Impact"), /*ErrorIfNotFound*/ false);
		UE_CLOG(!ImpactTag.IsValid(), LogNoiseEmitter, Warning,
				TEXT("%s: ImpactTag 가 비어 있고 Noise.Loot.Impact 도 없어 충돌 소음이 나지 않습니다."),
				*GetNameSafe(GetOwner()));
	}

	// 한 번 찾아서 붙들어 둔다. EndPlay 에서 다시 찾으면 파괴 도중이라 못 찾고
	// 엉뚱한 경고를 남기며, GetComponents 가 매번 힙 할당을 한다
	ImpactComponent = ResolveImpactComponent();

	if (UPrimitiveComponent* Impact = ImpactComponent.Get())
	{
		// 이걸 안 켜면 OnComponentHit 이 아예 오지 않는다.
		// 에디터 디테일 패널의 "Simulation Generates Hit Events" 와 같은 값이다
		Impact->SetNotifyRigidBodyCollision(true);
		Impact->OnComponentHit.AddDynamic(this, &UNoiseEmitterComponent::HandleHit);
	}
	else
	{
		UE_LOG(LogNoiseEmitter, Warning, TEXT("%s: 충돌을 감지할 프리미티브를 찾지 못했습니다."),
				*GetNameSafe(GetOwner()));
	}

	CacheImpactProfile();
	SpamFilter.Params = MakeFilterParams();
}

void UNoiseEmitterComponent::CacheImpactProfile()
{
	if (!ImpactTag.IsValid())
	{
		return;
	}

	UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this);
	const FNoiseProfileRow* Row = Subsystem ? Subsystem->FindProfile(ImpactTag) : nullptr;
	if (!Row)
	{
		// FindProfile 이 이미 태그당 한 번 경고했다
		return;
	}

	CachedMinImpulse      = Row->MinImpulse;
	CachedMaxImpulse      = Row->MaxImpulse;
	CachedCooldownSeconds = Row->CooldownSeconds;

	// 여기서 미리 로드한다. 히트 경로에서 LoadSynchronous 를 처음 부르면
	// 물리 콜백 한가운데서 동기 패키지 로드가 걸려 한 프레임 튄다
	CachedImpactCurve = Row->ImpactCurve.LoadSynchronous();

	bImpactProfileCached = true;
}

FNoiseSpamFilterParams UNoiseEmitterComponent::MakeFilterParams() const
{
	FNoiseSpamFilterParams Out;
	Out.ConsecutiveFalloff       = ConsecutiveFalloff;
	Out.ConsecutiveFloor         = ConsecutiveFloor;
	Out.ConsecutiveResetSeconds  = ConsecutiveResetSeconds;
	return Out;
}

void UNoiseEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPrimitiveComponent* Impact = ImpactComponent.Get())
	{
		Impact->OnComponentHit.RemoveDynamic(this, &UNoiseEmitterComponent::HandleHit);
	}
	ImpactComponent.Reset();

	// 발행을 틱으로 미루기 때문에, 소리를 낸 프레임에 그대로 파괴되면 대기 중인 소음이 사라진다.
	// 충돌로 깨지는 파손형 노획물(Noise.Loot.Break — 특대, 전 구역)이 정확히 이 경우라
	// 제일 커야 할 소음이 조용히 없어진다. 마지막 한 건은 여기서 내보낸다.
	//
	// 레벨 전환이나 종료 때는 내보내지 않는다 — 들을 사람도 없고 서브시스템도 정리 중이다
	if (EndPlayReason == EEndPlayReason::Destroyed && HasServerAuthority(this))
	{
		FlushPendingEmits();
	}

	SpamFilter.Reset();
	Modifiers.Reset();

	Super::EndPlay(EndPlayReason);
}

void UNoiseEmitterComponent::FlushPendingEmits()
{
	// 필터를 건드리기 전에 먼저 전부 뽑아낸다
	FNoisePendingEmitArray Flush;
	SpamFilter.FlushAll(Flush);

	for (const FNoisePendingEmit& Emit : Flush)
	{
		// 쿨다운을 내려줄 다음 틱이 없으므로 틱은 켜지 않는다
		EmitNow(Emit, /*bKeepTicking*/ false);
	}
}

UPrimitiveComponent* UNoiseEmitterComponent::ResolveImpactComponent() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	if (!ImpactComponentName.IsNone())
	{
		TArray<UPrimitiveComponent*> Primitives;
		Owner->GetComponents<UPrimitiveComponent>(Primitives);

		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive && Primitive->GetFName() == ImpactComponentName)
			{
				return Primitive;
			}
		}

		UE_LOG(LogNoiseEmitter, Warning, TEXT("%s: '%s' 컴포넌트를 찾지 못해 루트를 사용합니다."),
				*GetNameSafe(Owner), *ImpactComponentName.ToString());
	}

	return Cast<UPrimitiveComponent>(Owner->GetRootComponent());
}

// ──────────────────────────────────────────────────────────────
// 물리 충돌 → 소음
// ──────────────────────────────────────────────────────────────

void UNoiseEmitterComponent::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
									   UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 여기는 구르는 프롭 하나가 초당 수십 번 들어오는 경로다.
	// 캐시된 값만으로 임펄스 게이트를 먼저 통과시키고, 그보다 비싼 재질 쿼리는 그 뒤로 미룬다.
	// DataTable 조회는 BeginPlay 에서 이미 끝났다
	if (!HasServerAuthority(this) || !bImpactProfileCached)
	{
		return;
	}

	// 속도가 아니라 충격량을 쓴다 — 질량이 이미 반영돼 있어서
	// 무거운 금고와 가벼운 촛대가 같은 속도로 부딪혀도 소리가 다르게 난다
	const float Impulse = NormalImpulse.Size();

	const float Range  = FMath::Max(CachedMaxImpulse - CachedMinImpulse, KINDA_SMALL_NUMBER);
	const float Base01 = FMath::Clamp((Impulse - CachedMinImpulse) / Range, 0.f, 1.f);

	// MinImpulse / MaxImpulse 튜닝용. 콘솔에서 `Log LogNoiseEmitter Verbose` 로 켠다.
	// "소음이 왜 안 나지" 의 대부분은 여기서 Base01 이 0 인 경우다
	UE_LOG(LogNoiseEmitter, Verbose, TEXT("%s 충돌 — Impulse %.1f (Min %.1f / Max %.1f) -> Base01 %.3f"),
			*GetNameSafe(GetOwner()), Impulse, CachedMinImpulse, CachedMaxImpulse, Base01);

	if (Base01 <= 0.f)
	{
		return;   // MinImpulse 이하는 소음 없음. 재질 쿼리 전에 걸러진다
	}

	const float Shaped = ApplyImpactCurve(CachedImpactCurve, Base01);

	// 두 재질 중 작은 쪽이 이긴다 (흡음 우선).
	// 카펫 위에 떨어진 금고는 조용해야 직관적이다.
	//
	// 각자 자기 바디에서 읽는다. Hit.PhysMaterial 은 "맞은 지점" 의 재질이므로 상대 쪽에만 준다 —
	// 양쪽에 같이 주면 두 값이 같아져서 Min 이 아무 일도 하지 않는다
	const float Surface = FMath::Min(GetSurfaceCoeff(HitComponent, nullptr),
									 GetSurfaceCoeff(OtherComp,    Hit.PhysMaterial.Get()));

	// ImpactPoint 는 FVector_NetQuantize 라 명시적으로 변환한다
	FVector Location(Hit.ImpactPoint);
	if (!Hit.bBlockingHit && IsValid(GetOwner()))
	{
		// 접촉점이 없는 히트는 소유 액터 위치로 대신한다.
		// 예전에는 IsNearlyZero() 로 판정했는데 월드 원점 근처의 정상 충돌을 잡아먹었다
		Location = GetOwner()->GetActorLocation();
	}

	EmitThroughFilter(ImpactTag, Shaped * Surface, Location);
}

float UNoiseEmitterComponent::GetSurfaceCoeff(const UPrimitiveComponent* Component,
											  const UPhysicalMaterial* PreferredMaterial) const
{
	// UNoiseSettings::Get() 은 CDO 라 null 이 아니다 (NoiseSettings.h 주석 참고)
	return UNoiseSettings::Get()->GetSurfaceCoeff(ResolveSurface(Component, PreferredMaterial));
}

void UNoiseEmitterComponent::ReportTaggedNoise(FGameplayTag Tag, float LoudnessScale)
{
	if (!HasServerAuthority(this) || !Tag.IsValid())
	{
		return;
	}

	const FVector Location = IsValid(GetOwner()) ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	EmitThroughFilter(Tag, LoudnessScale, Location);
}

// ──────────────────────────────────────────────────────────────
// 모디파이어
// ──────────────────────────────────────────────────────────────

FGuid UNoiseEmitterComponent::AddModifier(const FNoiseModifier& Modifier)
{
	const FGuid Handle = FGuid::NewGuid();
	Modifiers.Add(Handle, Modifier);
	return Handle;
}

bool UNoiseEmitterComponent::RemoveModifier(FGuid Handle)
{
	return Modifiers.Remove(Handle) > 0;
}

// ──────────────────────────────────────────────────────────────
// 발행
// ──────────────────────────────────────────────────────────────

void UNoiseEmitterComponent::EmitThroughFilter(FGameplayTag Tag, float Loudness, const FVector& Location)
{
	// 1) 모디파이어 — 배율을 곱하고 필요하면 태그를 갈아끼운다
	FGameplayTag FinalTag   = Tag;
	float        Multiplier = 1.f;

	if (!Modifiers.IsEmpty())
	{
		const FGameplayTagContainer TagContainer(Tag);

		for (const TPair<FGuid, FNoiseModifier>& Pair : Modifiers)
		{
			const FNoiseModifier& Modifier = Pair.Value;

			// 쿼리가 비어 있으면 "전부 적용" 으로 본다.
			// FGameplayTagQuery::Matches 는 빈 쿼리에 대해 false 를 돌려주기 때문에 따로 처리한다
			if (!Modifier.AffectedTags.IsEmpty() && !Modifier.AffectedTags.Matches(TagContainer))
			{
				continue;
			}

			Multiplier *= Modifier.Multiplier;

			if (Modifier.OverrideTag.IsValid())
			{
				FinalTag = Modifier.OverrideTag;
			}
		}
	}

	const float Modified = FMath::Clamp(Loudness * Multiplier, 0.f, 1.f);
	if (Modified <= 0.f)
	{
		return;
	}

	// 2) 여기서 바로 발행하지 않고 필터에 접수만 한다. 실제 발행은 틱이 한다.
	//
	// 이 함수는 OnComponentHit 안에서 불린다. 그대로 발행하면
	//   물리 히트 콜백 → ReportNoise → Propagate → 청취자 수만큼 라인 트레이스
	// 가 전부 그 자리에서 동기로 돈다. 방 안에 노획물이 쏟아지는 프레임 —
	// 즉 제일 바쁜 순간에 비용이 몰리고, 분산할 자리도 없다.
	//
	// 틱으로 미루면 한 프레임의 충돌이 태그별로 하나씩 묶이고, 예산을 걸 자리도 생긴다.
	// 대가는 최대 1프레임 지연인데 경비 반응 기준으로는 체감되지 않는다.
	SpamFilter.Submit(FinalTag, Modified, Location);
	SetComponentTickEnabled(true);
}

void UNoiseEmitterComponent::EmitNow(const FNoisePendingEmit& Emit, bool bKeepTicking)
{
	UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	// 물리 히트 경로(ImpactTag)는 BeginPlay 에서 캐시해 둔 값을 쓴다.
	// ReportTaggedNoise 나 모디파이어 치환으로 들어온 다른 태그만 조회한다 —
	// 던지기 · 파괴는 초당 수십 번 일어나지 않는다
	float CooldownSeconds = FallbackCooldownSeconds;
	if (bImpactProfileCached && Emit.Tag == ImpactTag)
	{
		CooldownSeconds = CachedCooldownSeconds;
	}
	else if (const FNoiseProfileRow* Row = Subsystem->FindProfile(Emit.Tag))
	{
		CooldownSeconds = Row->CooldownSeconds;
	}

	// 필터 갱신은 ReportNoise 보다 먼저 끝낸다.
	// ReportNoise → 청취자 반응 → 같은 컴포넌트의 ReportTaggedNoise 로 재진입할 수 있는데,
	// 그때 필터 내부 상태가 어중간하면 안 된다.
	// (NotifyEmitted 가 내부 참조를 밖으로 내보내지 않으므로 무효 참조 위험 자체가 없다)
	const int32 ConsecutiveCount = SpamFilter.NotifyEmitted(Emit.Tag, Emit.Loudness, CooldownSeconds);

	// 스팸 필터가 실제로 몇 건을 통과시켰는지 보려면 이 줄이 필요하다.
	// 충돌 횟수와 발행 횟수가 같으면 필터가 새고 있는 것이다
	UE_LOG(LogNoiseEmitter, Verbose, TEXT("발행: %s Loudness %.3f AlertScale %.2f (연속 %d회, 쿨다운 %.2fs)"),
			*Emit.Tag.ToString(), Emit.Loudness, Emit.AlertScale, ConsecutiveCount, CooldownSeconds);

	// Instigator 는 소유 액터. 던져진 노획물이면 김민준 쪽에서 SetInstigator 로
	// 던진 플레이어를 심어줘야 "최다 소음 유발자" 집계에 잡힌다
	Subsystem->ReportNoise(Emit.Tag, Emit.Location, Emit.Loudness, GetOwner(), Emit.AlertScale);

	if (bKeepTicking)
	{
		SetComponentTickEnabled(true);   // 쿨다운을 내려야 한다
	}
}

void UNoiseEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType,
										   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasServerAuthority(this) || SpamFilter.IsEmpty())
	{
		SetComponentTickEnabled(false);
		return;
	}

	// 디자이너가 PIE 중에 디테일 패널에서 만질 수 있다. POD 3개 복사라 비용은 없다
	SpamFilter.Params = MakeFilterParams();

	FNoisePendingEmitArray Pending;
	const bool bAnyActive = SpamFilter.Advance(DeltaTime, Pending);

	// 필터 순회가 끝난 뒤에 발행한다 — 청취자가 반응하다가 새 소음을 내면
	// Advance 가 순회 중이던 TMap 이 재할당되어 이터레이터가 날아간다
	for (const FNoisePendingEmit& Emit : Pending)
	{
		EmitNow(Emit, /*bKeepTicking*/ true);
	}

	// 발행이 있었으면 Advance 가 그 상태를 살려뒀으므로 bAnyActive 도 참이다
	if (!bAnyActive)
	{
		SetComponentTickEnabled(false);
	}
}
