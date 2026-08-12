#include "Noise/NoiseTestProp.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

#include "Noise/NoiseEmitterComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoiseProp, Log, All);

ANoiseTestProp::ANoiseTestProp()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);   // 클라 창에서도 굴러가는 게 보이도록

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Body->SetStaticMesh(CubeMesh.Object);
	}

	// PhysicsActor 가 아니라 Loot 이어야 한다.
	// PhysicsActor 는 CustomResponses 가 비어 있어서 NoiseOcclusion 채널 기본값(Block)을 그대로 쓴다.
	// 그러면 테스트 상자가 스스로 차폐물이 되어 실제 노획물과 다른 감쇄 거동을 보인다
	Body->SetCollisionProfileName(TEXT("Loot"));
	Body->SetSimulatePhysics(true);

	// 이걸 안 켜면 OnComponentHit 이 아예 오지 않는다.
	// UNoiseEmitterComponent 가 BeginPlay 에서 다시 켜주지만, 여기서도 명시해둔다
	Body->SetNotifyRigidBodyCollision(true);

	// 중량형 노획물 흉내. 충격량은 질량에 비례하므로 이 값이 소리 크기를 좌우한다
	Body->SetMassOverrideInKg(NAME_None, 40.f);

	NoiseEmitter = CreateDefaultSubobject<UNoiseEmitterComponent>(TEXT("NoiseEmitter"));
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 콘솔 명령. 쉬핑 빌드에서는 코드째 빠진다
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

static void NoiseSpawnPropCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (World->IsNetMode(NM_Client))
	{
		UE_LOG(LogNoiseProp, Warning, TEXT("물리와 소음은 서버 권위입니다. 서버 창에서 실행하세요."));
		return;
	}

	const APlayerController* Controller = World->GetFirstPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogNoiseProp, Warning, TEXT("플레이어 폰을 찾지 못했습니다."));
		return;
	}

	const float Height = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 500.f;

	const FVector Location = Pawn->GetActorLocation()
						   + Pawn->GetActorForwardVector() * 250.f
						   + FVector(0.f, 0.f, Height);

	FActorSpawnParameters Params;
	// 이 한 줄이 "최다 소음 유발자" 집계를 돌게 한다.
	// UAlertComponent 가 Instigator 액터 -> GetInstigatorController() -> PlayerState 로 역추적한다
	Params.Instigator = Pawn;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (World->SpawnActor<ANoiseTestProp>(ANoiseTestProp::StaticClass(), Location, FRotator::ZeroRotator, Params))
	{
		UE_LOG(LogNoiseProp, Log, TEXT("테스트 상자를 %.0f 유닛 상공에 생성했습니다. 떨어지면 충돌 소음이 발행됩니다."), Height);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GNoiseSpawnPropCommand(
	  TEXT("hh.Noise.SpawnProp"),
	  TEXT("hh.Noise.SpawnProp [높이] — 플레이어 앞 상공에 물리 상자를 떨어뜨린다. 기본 500"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NoiseSpawnPropCommand),
	  ECVF_Cheat);

static void NoiseClearPropsCommand(UWorld* World)
{
	if (!World)
	{
		return;
	}

	int32 Count = 0;
	for (TActorIterator<ANoiseTestProp> It(World); It; ++It)
	{
		It->Destroy();
		++Count;
	}

	UE_LOG(LogNoiseProp, Log, TEXT("테스트 상자 %d 개를 제거했습니다."), Count);
}

static FAutoConsoleCommandWithWorld GNoiseClearPropsCommand(
	  TEXT("hh.Noise.ClearProps"),
	  TEXT("생성된 테스트 상자를 전부 제거한다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&NoiseClearPropsCommand),
	  ECVF_Cheat);

#endif   // !UE_BUILD_SHIPPING
