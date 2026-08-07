#include "Noise/PerceptionMeterComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#include "Noise/NoiseSubsystem.h"

UPerceptionMeterComponent::UPerceptionMeterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 게이지가 0 인 동안은 돌 필요가 없다. 자극을 받을 때 켠다.
	// 경비 10명이 매 프레임 도는 것과 아닌 것의 차이다
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// 빼면 컴파일 에러도 경고도 없이 클라이언트 게이지만 안 움직인다 (팀 컨벤션 3-3)
	SetIsReplicatedByDefault(true);
}

void UPerceptionMeterComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPerceptionMeterComponent, Perception01);
}

bool UPerceptionMeterComponent::HasNoiseAuthority() const
{
	return GetOwnerRole() == ROLE_Authority;
}

// ──────────────────────────────────────────────────────────────
// 등록 / 해제
// ──────────────────────────────────────────────────────────────

void UPerceptionMeterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasNoiseAuthority())
	{
		return;   // 소음 판정은 서버 전용. 클라는 복제된 값만 받는다
	}

	if (UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this))
	{
		Subsystem->RegisterListener(this);
	}
}

void UPerceptionMeterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 서브시스템이 약참조라 안 지워도 크래시는 안 나지만,
	// 경비가 죽고 리스폰될 때마다 죽은 항목이 배열에 쌓인다
	if (HasNoiseAuthority())
	{
		if (UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this))
		{
			Subsystem->UnregisterListener(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// ──────────────────────────────────────────────────────────────
// INoiseListener
// ──────────────────────────────────────────────────────────────

FVector UPerceptionMeterComponent::GetListenerLocation_Implementation() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FVector::ZeroVector;
	}

	// 폰이면 시점 위치가 머리 높이에 가장 가깝다
	if (const APawn* OwnerPawn = Cast<const APawn>(Owner))
	{
		return OwnerPawn->GetPawnViewLocation();
	}

	return Owner->GetActorLocation() + FVector(0.f, 0.f, EarHeight);
}

void UPerceptionMeterComponent::OnNoiseHeard_Implementation(const FNoiseStimulus& Stimulus)
{
	if (!HasNoiseAuthority() || bLatched)
	{
		return;
	}

	LastNoiseLocation     = Stimulus.Location;
	TimeSinceLastStimulus = 0.f;

	// 감소 판정을 돌리려면 틱이 필요하다. SetPerception 보다 먼저 켜야
	// 래치되는 경우에 SetPerception 이 도로 꺼줄 수 있다
	SetComponentTickEnabled(true);

	SetPerception(Perception01 + Stimulus.Strength * GainPerStimulus);
}

// ──────────────────────────────────────────────────────────────
// 게이지
// ──────────────────────────────────────────────────────────────

void UPerceptionMeterComponent::SetPerception(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Clamped, Perception01))
	{
		return;
	}

	Perception01 = Clamped;

	// 서버에서는 RepNotify 가 호출되지 않는다.
	// 리슨 서버의 호스트 창에서도 위젯이 갱신되려면 직접 불러야 한다
	OnRep_Perception();

	if (Perception01 >= 1.f && !bLatched)
	{
		bLatched = true;
		SetComponentTickEnabled(false);   // 래치 중엔 오르지도 내리지도 않는다
		OnPerceptionFull.Broadcast(LastNoiseLocation);
	}
	else if (Perception01 <= 0.f)
	{
		SetComponentTickEnabled(false);   // 더 내려갈 것이 없다
	}
}

void UPerceptionMeterComponent::OnRep_Perception()
{
	OnPerceptionChanged.Broadcast(Perception01);
}

void UPerceptionMeterComponent::ResetPerception()
{
	if (!HasNoiseAuthority())
	{
		return;
	}

	bLatched              = false;
	TimeSinceLastStimulus = 0.f;

	SetPerception(0.f);
	SetComponentTickEnabled(false);
}

// ──────────────────────────────────────────────────────────────
// 감소
// ──────────────────────────────────────────────────────────────

void UPerceptionMeterComponent::TickComponent(float DeltaTime, ELevelTick TickType,
																						FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasNoiseAuthority() || bLatched || Perception01 <= 0.f)
	{
		SetComponentTickEnabled(false);
		return;
	}

	TimeSinceLastStimulus += DeltaTime;
	if (TimeSinceLastStimulus < DecayGraceSeconds)
	{
		return;
	}

	SetPerception(Perception01 - DecayPerSecond * DeltaTime);
}