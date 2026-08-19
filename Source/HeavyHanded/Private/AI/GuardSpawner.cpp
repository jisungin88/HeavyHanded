#include "AI/GuardSpawner.h"
#include "AI/GuardTypes.h"
#include "Alert/AlertComponent.h"
#include "Character/GuardCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AGuardSpawner::AGuardSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnRoot"));
	RootComponent = SpawnRoot;
}

void AGuardSpawner::BeginPlay()
{
	Super::BeginPlay();

	// 경비는 서버 권위 폰이다 - 클라에서 스폰하면 복제 소유권이 어긋난다.
	if (!HasAuthority())
	{
		return;
	}

	// NoiseSubsystem::Initialize 가 GameState 에 AlertComponent 를 붙이는 시점은 보통
	// 액터 BeginPlay 보다 먼저다(GameStateSetEvent 로 늦게 붙는 경우까지 포함). 그래도
	// 못 찾으면 이 스포너는 그냥 병력 증원을 못 받을 뿐, 초기 스폰은 정상 진행한다.
	if (UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		Alert->OnReinforcementTriggered.AddDynamic(this, &AGuardSpawner::HandleReinforcement);
	}
	else
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] AlertComponent 를 찾지 못해 병력 증원을 구독하지 못했다."), *GetName());
	}

	if (bSpawnOnBeginPlay)
	{
		SpawnGuards();
	}
}

void AGuardSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAlertComponent* Alert = UAlertComponent::Get(this))
	{
		Alert->OnReinforcementTriggered.RemoveDynamic(this, &AGuardSpawner::HandleReinforcement);
	}

	Super::EndPlay(EndPlayReason);
}

void AGuardSpawner::HandleReinforcement(int32 ReinforcementIndex)
{
	UE_LOG(LogGuardAI, Log, TEXT("[%s] 병력 증원 %d번째 - 경비 1명 추가 스폰."), *GetName(), ReinforcementIndex);

	// GuardCount 를 같이 올려서, 이후 로그의 "몇/몇 스폰" 수치와 남은 스폰 예정 수가 계속 맞게 한다.
	++GuardCount;
	SpawnSingleGuard();
}

void AGuardSpawner::SpawnGuards()
{
	if (!HasAuthority())
	{
		UE_LOG(LogGuardAI, Warning, TEXT("[%s] 서버가 아니라 경비를 스폰하지 않는다."), *GetName());
		return;
	}

	if (!GuardClass)
	{
		UE_LOG(LogGuardAI, Error, TEXT("[%s] GuardClass 가 비어 있다."), *GetName());
		return;
	}

	if (PatrolPoints.Num() == 0)
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] PatrolPoints 가 비어 있다 - 스폰된 경비가 순찰 없이 제자리에 대기한다."), *GetName());
	}

	// SpawnGuards() 를 다시 호출할 수 있으니(BlueprintCallable) 이전 타이머·진행 상태를 지운다.
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	SpawnedCount = 0;

	if (GuardCount <= 0)
	{
		return;
	}

	// 첫 1명은 즉시 스폰한다 - "지금 배치" 느낌을 주려고 5초를 기다리게 하지 않는다.
	SpawnSingleGuard();

	if (SpawnedCount < GuardCount)
	{
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this,
			&AGuardSpawner::HandleSpawnTimer, FMath::Max(SpawnInterval, 0.01f), true);
	}
	else
	{
		UE_LOG(LogGuardAI, Log, TEXT("[%s] 경비 %d/%d명 스폰 완료 (순찰 지점 %d개, 패턴은 경비마다 무작위)."),
			*GetName(), SpawnedCount, GuardCount, PatrolPoints.Num());
	}
}

void AGuardSpawner::HandleSpawnTimer()
{
	SpawnSingleGuard();

	if (SpawnedCount >= GuardCount)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

		UE_LOG(LogGuardAI, Log, TEXT("[%s] 경비 %d/%d명 스폰 완료 (순찰 지점 %d개, 패턴은 경비마다 무작위)."),
			*GetName(), SpawnedCount, GuardCount, PatrolPoints.Num());
	}
}

void AGuardSpawner::SpawnSingleGuard()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// Deferred 스폰: PatrolPoints/PatrolPattern 은 AI 빙의(OnPossess) 전에 채워야 한다.
	// FinishSpawning 이후에 넣으면 이미 AutoPossessAI 로 빙의가 끝난 뒤라, 첫
	// SelectNextPatrolPoint 호출이 빈 배열을 보고 경고만 남기고 돌아간다.
	AGuardCharacter* Guard = World->SpawnActorDeferred<AGuardCharacter>(
		GuardClass, GetActorTransform(), this, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!IsValid(Guard))
	{
		UE_LOG(LogGuardAI, Error, TEXT("[%s] 경비 스폰 실패 (%d/%d번째)."), *GetName(), SpawnedCount + 1, GuardCount);
		return;
	}

	Guard->PatrolPoints = PatrolPoints;
	Guard->PatrolPattern = DrawNextPatrolPattern();

	Guard->FinishSpawning(GetActorTransform());

	// AutoPossessAI 가 "Placed in World Only"면 런타임 스폰 폰은 자동 빙의되지 않는다.
	// BP 설정에 기대지 않고 여기서 직접 보장한다 - SpawnDefaultController()는
	// AutoPossessAI가 실제로 호출하는 것과 같은 함수라 AIControllerClass를 그대로 존중한다.
	// 이미 빙의돼 있으면(AutoPossessAI가 Spawned 계열이면) 아무 일도 하지 않는다.
	if (!IsValid(Guard->GetController()))
	{
		Guard->SpawnDefaultController();

		if (!IsValid(Guard->GetController()))
		{
			UE_LOG(LogGuardAI, Error,
				TEXT("[%s] %s 빙의 실패 - AIControllerClass 가 비어 있는지 확인할 것."),
				*GetName(), *Guard->GetName());
		}
	}

	++SpawnedCount;
}

EPatrolPattern AGuardSpawner::DrawNextPatrolPattern()
{
	if (PatternBag.Num() == 0)
	{
		PatternBag = { EPatrolPattern::Loop, EPatrolPattern::PingPong, EPatrolPattern::Random };

		// Fisher-Yates 셔플.
		for (int32 Index = PatternBag.Num() - 1; Index > 0; --Index)
		{
			PatternBag.Swap(Index, FMath::RandRange(0, Index));
		}

		// 가방 경계: 방금 다 비운 가방의 마지막 패턴과 새로 채운 가방의 첫 패턴이
		// 같으면 두 경비가 연속으로 같은 패턴을 받는다. 다른 자리와 바꿔서 막는다.
		if (bHasDrawnPattern && PatternBag[0] == LastDrawnPattern && PatternBag.Num() > 1)
		{
			PatternBag.Swap(0, FMath::RandRange(1, PatternBag.Num() - 1));
		}
	}

	LastDrawnPattern = PatternBag.Pop();
	bHasDrawnPattern = true;

	return LastDrawnPattern;
}
