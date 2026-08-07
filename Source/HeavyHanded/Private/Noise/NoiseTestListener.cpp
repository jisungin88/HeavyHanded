#include "Noise/NoiseTestListener.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"                  // TActorIterator — 아래 콘솔 명령에서 쓴다
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

#include "Noise/PerceptionMeterComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoiseTest, Log, All);

ANoiseTestListener::ANoiseTestListener()
{
	PrimaryActorTick.bCanEverTick = true;

	// 컴포넌트의 Perception01 이 클라로 가려면 소유 액터가 복제돼야 한다.
	// 이거 빼면 컴포넌트에 SetIsReplicatedByDefault 를 해도 클라 게이지가 0 에 멈춘다
	bReplicates = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Body->SetStaticMesh(CubeMesh.Object);
	}

	// 더미 몸통이 플레이어 이동이나 트레이스를 방해하지 않게 한다
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PerceptionMeter = CreateDefaultSubobject<UPerceptionMeterComponent>(TEXT("PerceptionMeter"));
}

void ANoiseTestListener::BeginPlay()
{
	Super::BeginPlay();

	if (PerceptionMeter)
	{
		PerceptionMeter->OnPerceptionFull.AddDynamic(this, &ANoiseTestListener::HandlePerceptionFull);
	}
}

void ANoiseTestListener::HandlePerceptionFull(FVector LastNoiseLocation)
{
	UE_LOG(LogNoiseTest, Warning, TEXT("인지 100%% 도달 — 조사 지점 %s"), *LastNoiseLocation.ToString());

#if ENABLE_DRAW_DEBUG
	DrawDebugSphere(GetWorld(), LastNoiseLocation, 60.f, 12, FColor::Red, false, 5.f);
#endif
}

void ANoiseTestListener::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if ENABLE_DRAW_DEBUG
	if (!PerceptionMeter)
	{
		return;
	}

	const float Value = PerceptionMeter->GetPerception01();

	const FString Text = FString::Printf(TEXT("%s  %3.0f%%%s"),
			HasAuthority() ? TEXT("[서버]") : TEXT("[클라]"),
			Value * 100.f,
			PerceptionMeter->IsLatched() ? TEXT("  LATCHED") : TEXT(""));

	// 세 번째 인자를 액터로 주면 위치가 액터 기준 상대 좌표가 된다
	DrawDebugString(GetWorld(), FVector(0.f, 0.f, 120.f), Text, this,
									Value >= 1.f ? FColor::Red : FColor::Yellow, 0.f, true);
#endif
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 래치 해제. 경비 BT 가 ResetPerception() 을 부르기 전까지의 대역이다
// ──────────────────────────────────────────────────────────────
static void NoiseResetListenersCommand(UWorld* World)
{
	if (!World)
	{
		return;
	}

	int32 Count = 0;
	for (TActorIterator<ANoiseTestListener> It(World); It; ++It)
	{
		if (UPerceptionMeterComponent* Meter = It->FindComponentByClass<UPerceptionMeterComponent>())
		{
			Meter->ResetPerception();
			++Count;
		}
	}

	UE_LOG(LogNoiseTest, Log, TEXT("%d 개 청취자를 리셋했습니다."), Count);
}

static FAutoConsoleCommandWithWorld GNoiseResetListenersCommand(
	  TEXT("hh.Noise.ResetListeners"),
	  TEXT("배치된 NoiseTestListener 의 인지 게이지를 전부 0 으로 되돌린다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&NoiseResetListenersCommand));