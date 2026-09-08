#include "Hazards/Puddle.h"

#include "Character/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"      // UGameplayEffect 완전한 타입 — TSubclassOf 의 StaticClass() 호출에 필요하다
#include "Hazards/HazardLog.h"

APuddle::APuddle()
{
	PrimaryActorTick.bCanEverTick = false;

	PuddleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PuddleMesh"));
	SetRootComponent(PuddleMesh);

	// 시각 전용이다. 밟아서 막히면 안 되므로 콜리전을 끈다 — 판정은 SlowZone 이 한다
	PuddleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// AMovementTrap 과 같은 이유로 폰 채널만 오버랩으로 연다 — 노획물을 던져 넣어도 반응하지 않는다
	SlowZone = CreateDefaultSubobject<UBoxComponent>(TEXT("SlowZone"));
	SlowZone->SetupAttachment(PuddleMesh);
	SlowZone->SetBoxExtent(FVector(100.f, 100.f, 50.f));
	SlowZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlowZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	SlowZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SlowZone->SetGenerateOverlapEvents(true);
	SlowZone->OnComponentBeginOverlap.AddDynamic(this, &APuddle::OnZoneBeginOverlap);
	SlowZone->OnComponentEndOverlap.AddDynamic(this, &APuddle::OnZoneEndOverlap);
}

void APuddle::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "안에 들어갔는데 아무 일도 안 일어난다" 하나로만 드러난다
	// (ABreakableWall::BeginPlay 와 동일 사유)
	if (!IsValid(PuddleMesh) || !PuddleMesh->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[Puddle:%s] PuddleMesh 에 메시가 없다 — 눈에 안 보이는 웅덩이가 된다"), *GetName());
	}

	if (!SlowEffectClass)
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[Puddle:%s] SlowEffectClass 가 비어 있다 — 들어가도 느려지지 않는다"), *GetName());
	}
}

void APuddle::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 서버 권위 판정 (AMovementTrap 과 동일 사유)
	if (!HasAuthority())
	{
		return;
	}

	// AGuardCharacter 는 안 걸린다 — AMovementTrap 과 동일 사유
	ABaseCharacter* Target = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(Target) || !SlowEffectClass)
	{
		return;
	}

	Target->ApplyGameplayEffectToSelf(SlowEffectClass);

	UE_LOG(LogHazard, Log, TEXT("[Puddle:%s] %s 진입 — 감속 적용"), *GetName(), *Target->GetName());
}

void APuddle::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	ABaseCharacter* Target = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(Target) || !SlowEffectClass)
	{
		return;
	}

	Target->RemoveGameplayEffectFromSelf(SlowEffectClass);

	UE_LOG(LogHazard, Log, TEXT("[Puddle:%s] %s 이탈 — 감속 해제"), *GetName(), *Target->GetName());
}
