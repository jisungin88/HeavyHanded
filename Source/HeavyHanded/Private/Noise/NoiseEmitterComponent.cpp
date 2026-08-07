#include "Noise/NoiseEmitterComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"

#include "Noise/NoiseSettings.h"
#include "Noise/NoiseSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoiseEmitter, Log, All);

namespace
{
	/** 프로파일에 커브가 없으면 선형으로 둔다 */
	float ApplyImpactCurve(const FNoiseProfileRow& Row, float Base01)
	{
		if (const UCurveFloat* Curve = Row.ImpactCurve.LoadSynchronous())
		{
			return FMath::Clamp(Curve->GetFloatValue(Base01), 0.f, 1.f);
		}
		return Base01;
	}

	/**
	 * 프리미티브의 물리 표면을 얻는다.
	 * 시뮬레이션 히트 이벤트의 FHitResult 는 PhysMaterial 이 비어 있는 경우가 많아서
	 * (bReturnPhysicalMaterial 은 트레이스 쿼리 옵션이다) 바디 인스턴스에서 직접 읽는다.
	 */
	EPhysicalSurface ResolveSurface(const UPrimitiveComponent* Component, const FHitResult& Hit)
	{
		if (Hit.PhysMaterial.IsValid())
		{
			return UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
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

bool UNoiseEmitterComponent::HasNoiseAuthority() const
{
	return GetOwnerRole() == ROLE_Authority;
}

void UNoiseEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasNoiseAuthority())
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

	if (UPrimitiveComponent* Impact = ResolveImpactComponent())
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
}

void UNoiseEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPrimitiveComponent* Impact = ResolveImpactComponent())
	{
		Impact->OnComponentHit.RemoveDynamic(this, &UNoiseEmitterComponent::HandleHit);
	}

	EmitStates.Reset();
	Modifiers.Reset();

	Super::EndPlay(EndPlayReason);
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
	if (!HasNoiseAuthority() || !ImpactTag.IsValid())
	{
		return;
	}

	UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	const FNoiseProfileRow* Row = Subsystem->FindProfile(ImpactTag);
	if (!Row)
	{
		return;
	}

	// 속도가 아니라 충격량을 쓴다 — 질량이 이미 반영돼 있어서
	// 무거운 금고와 가벼운 촛대가 같은 속도로 부딪혀도 소리가 다르게 난다
	const float Impulse = NormalImpulse.Size();

	const float Range  = FMath::Max(Row->MaxImpulse - Row->MinImpulse, KINDA_SMALL_NUMBER);
	const float Base01 = FMath::Clamp((Impulse - Row->MinImpulse) / Range, 0.f, 1.f);

	// MinImpulse / MaxImpulse 튜닝용. 콘솔에서 `Log LogNoiseEmitter Verbose` 로 켠다.
	// "소음이 왜 안 나지" 의 대부분은 여기서 Base01 이 0 인 경우다
	UE_LOG(LogNoiseEmitter, Verbose, TEXT("%s 충돌 — Impulse %.1f (Min %.1f / Max %.1f) -> Base01 %.3f"),
			*GetNameSafe(GetOwner()), Impulse, Row->MinImpulse, Row->MaxImpulse, Base01);

	if (Base01 <= 0.f)
	{
		return;   // MinImpulse 이하는 소음 없음
	}

	const float Shaped = ApplyImpactCurve(*Row, Base01);

	// 두 재질 중 작은 쪽이 이긴다 (흡음 우선).
	// 카펫 위에 떨어진 금고는 조용해야 직관적이다
	const float Surface = FMath::Min(GetSurfaceCoeff(HitComponent, Hit),
									 GetSurfaceCoeff(OtherComp,    Hit));

	// ImpactPoint 는 FVector_NetQuantize 라 명시적으로 변환한다
	FVector Location(Hit.ImpactPoint);
	if (Location.IsNearlyZero() && IsValid(GetOwner()))
	{
		Location = GetOwner()->GetActorLocation();
	}

	EmitThroughFilter(ImpactTag, Shaped * Surface, Location);
}

float UNoiseEmitterComponent::GetSurfaceCoeff(const UPrimitiveComponent* Component, const FHitResult& Hit) const
{
	const UNoiseSettings* Settings = UNoiseSettings::Get();
	if (!Settings)
	{
		return 1.f;
	}

	return Settings->GetSurfaceCoeff(ResolveSurface(Component, Hit));
}

void UNoiseEmitterComponent::ReportTaggedNoise(FGameplayTag Tag, float LoudnessScale)
{
	if (!HasNoiseAuthority() || !Tag.IsValid())
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
// 스팸 필터
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

	// 2) 연속 충돌 체감 — 굴러다니는 와인 랙이 계속 같은 크기로 울리지 않게 한다
	FNoiseEmitState& State = EmitStates.FindOrAdd(FinalTag);

	const float Falloff = FMath::Max(FMath::Pow(ConsecutiveFalloff, static_cast<float>(State.ConsecutiveHits)),
									 ConsecutiveFloor);
	const float Damped  = Modified * Falloff;

	++State.ConsecutiveHits;
	State.TimeSinceLastHit = 0.f;
	State.PendingLocation  = Location;

	// 3) 쿨다운 중이면 발행하지 않고 가장 큰 값만 기억한다
	if (State.CooldownRemaining > 0.f)
	{
		State.PendingMaxLoudness = FMath::Max(State.PendingMaxLoudness, Damped);
		SetComponentTickEnabled(true);
		return;
	}

	EmitNow(FinalTag, Damped, Location);
}

void UNoiseEmitterComponent::EmitNow(FGameplayTag Tag, float Loudness, const FVector& Location)
{
	UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	const FNoiseProfileRow* Row = Subsystem->FindProfile(Tag);

	FNoiseEmitState& State = EmitStates.FindOrAdd(Tag);
	State.LastEmittedLoudness = Loudness;
	State.PendingMaxLoudness  = 0.f;
	State.CooldownRemaining   = Row ? Row->CooldownSeconds : 0.25f;

	// 스팸 필터가 실제로 몇 건을 통과시켰는지 보려면 이 줄이 필요하다.
	// 충돌 횟수와 발행 횟수가 같으면 필터가 새고 있는 것이다
	UE_LOG(LogNoiseEmitter, Verbose, TEXT("발행: %s Loudness %.3f (연속 %d회, 쿨다운 %.2fs)"),
			*Tag.ToString(), Loudness, State.ConsecutiveHits, State.CooldownRemaining);

	// Instigator 는 소유 액터. 던져진 노획물이면 김민준 쪽에서 SetInstigator 로
	// 던진 플레이어를 심어줘야 "최다 소음 유발자" 집계에 잡힌다
	Subsystem->ReportNoise(Tag, Location, Loudness, GetOwner());

	SetComponentTickEnabled(true);
}

void UNoiseEmitterComponent::TickComponent(float DeltaTime, ELevelTick TickType,
										   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasNoiseAuthority() || EmitStates.IsEmpty())
	{
		SetComponentTickEnabled(false);
		return;
	}

	struct FPendingEmit
	{
		FGameplayTag Tag;
		float        Loudness;
		FVector      Location;
	};

	TArray<FPendingEmit, TInlineAllocator<4>> Pending;
	bool bAnyActive = false;

	for (TMap<FGameplayTag, FNoiseEmitState>::TIterator It(EmitStates); It; ++It)
	{
		FNoiseEmitState& State = It.Value();

		State.TimeSinceLastHit += DeltaTime;

		if (State.CooldownRemaining > 0.f)
		{
			State.CooldownRemaining -= DeltaTime;

			if (State.CooldownRemaining <= 0.f)
			{
				State.CooldownRemaining = 0.f;

				// 쿨다운 중에 더 큰 소리가 있었으면 차액만 내보낸다.
				// 전체를 다시 내보내면 한 번 굴린 걸로 경보가 차버린다
				if (State.PendingMaxLoudness > State.LastEmittedLoudness)
				{
					Pending.Add({ It.Key(),
								  State.PendingMaxLoudness - State.LastEmittedLoudness,
								  State.PendingLocation });
				}
				State.PendingMaxLoudness = 0.f;
			}
		}

		if (State.TimeSinceLastHit >= ConsecutiveResetSeconds)
		{
			State.ConsecutiveHits     = 0;
			State.LastEmittedLoudness = 0.f;
		}

		const bool bIdle = State.CooldownRemaining <= 0.f
						&& State.ConsecutiveHits == 0
						&& State.PendingMaxLoudness <= 0.f;

		if (bIdle)
		{
			It.RemoveCurrent();
			continue;
		}

		bAnyActive = true;
	}

	// 맵 순회가 끝난 뒤에 발행한다 — 청취자가 반응하다가 새 소음을 내면
	// 순회 중이던 TMap 이 재할당되어 참조가 날아간다
	for (const FPendingEmit& Emit : Pending)
	{
		EmitNow(Emit.Tag, Emit.Loudness, Emit.Location);
	}

	if (!bAnyActive && Pending.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}
