#include "Hazards/CreakyFloor.h"

#include "Character/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"   // FGameplayTag::RequestGameplayTag — 임시 문자열 조회용
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "Noise/NoiseSubsystem.h"

ACreakyFloor::ACreakyFloor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Multicast_PlayCreakSound 를 쓰려면 복제 액터여야 한다
	bReplicates = true;

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	SetRootComponent(FloorMesh);

	// 시각 전용이다. 바닥은 레벨의 실제 지오메트리가 담당한다 (APuddle::PuddleMesh 와 동일 사유)
	FloorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// AMovementTrap/APuddle 과 같은 이유로 폰 채널만 오버랩으로 연다
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(FloorMesh);
	TriggerVolume->SetBoxExtent(FVector(50.f, 50.f, 30.f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &ACreakyFloor::OnTriggerOverlap);
}

void ACreakyFloor::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "밟았는데 아무 일도 안 일어난다" 하나로만 드러난다 (ABreakableWall::BeginPlay 와 동일 사유)
	if (!IsValid(FloorMesh) || !FloorMesh->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[CreakyFloor:%s] FloorMesh 에 메시가 없다 — 눈에 안 보이는 마루가 된다"), *GetName());
	}
}

void ACreakyFloor::OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 서버 권위 판정 (AMovementTrap 과 동일 사유)
	if (!HasAuthority())
	{
		return;
	}

	// AGuardCharacter 는 별도 클래스라 여기 안 걸린다 — 경비가 자기 순찰 중 마루를
	// 스스로 울리지 않는다 (AMovementTrap 과 동일 사유)
	ABaseCharacter* Target = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(Target))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (LastTriggerTime >= 0.f && Now - LastTriggerTime < MinRetriggerInterval)
	{
		// 오버랩 경계에서 스치듯 들락거린 것 — 진짜 재진입이 아니다
		return;
	}
	LastTriggerTime = Now;

	// 기획서 6장 — 밟은 사람이 Instigator. Noise.ini(지성인)에 이미 등록된 태그를 참조만 한다.
	//
	// [임시: 네이티브 선언 대신 문자열 조회]
	//   HeavyHandedGameplayTags.h/.cpp 를 환경 방해 요소 작업 이전 상태로 되돌려 달라는
	//   요청(공용 파일 충돌 우려)에 따라 HHTags::Noise_Environment_CreakyFloor 선언을 걷어냈다.
	//   기능은 그대로 유지하려고 문자열 조회로 임시 대체한 것 — 복구 요청이 오면
	//   HeavyHandedGameplayTags.h 에 다시 선언하고 이 줄을 HHTags::Noise_Environment_CreakyFloor 로 되돌릴 것.
	if (UNoiseSubsystem* Noise = UNoiseSubsystem::Get(this))
	{
		static const FGameplayTag CreakNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Environment.CreakyFloor"));
		Noise->ReportNoise(CreakNoiseTag, GetActorLocation(), 1.f, Target);
	}

	Multicast_PlayCreakSound();

	UE_LOG(LogHazard, Log, TEXT("[CreakyFloor:%s] %s 가 밟음 — 소음 발행"), *GetName(), *Target->GetName());
}

void ACreakyFloor::Multicast_PlayCreakSound_Implementation()
{
	const UWorld* World = GetWorld();

	// 데디케이티드 서버는 화면도 스피커도 없다 (AMovementTrap::Multicast_PlayTriggerEffect 와 동일 사유)
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (IsValid(CreakSound))
	{
		UGameplayStatics::PlaySoundAtLocation(World, CreakSound, GetActorLocation());
	}
}
