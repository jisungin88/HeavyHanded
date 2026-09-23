#include "Hazards/SecurityCamera.h"

#include "Alert/AlertComponent.h"
#include "Character/BaseCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator — 소수의 캐릭터를 매 판정마다 순회한다 (AStickyBomb 와 동일 사유)
#include "GameFramework/GameStateBase.h"
#include "Hazards/HazardLog.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// [디버그 전용] 왜 안 걸리는지(거리 초과/콘 밖/시야 차단) 눈으로 구분이 안 돼서 만든 진단
// 도구다. 쉬핑 빌드에서는 코드째 빠진다 (ABreakableWall 의 hh.Hazard.BreakWall 과 동일 사유)
#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<bool> CVarHazardCameraDebug(
	TEXT("hh.Hazard.CameraDebug"),
	false,
	TEXT("true 면 SecurityCamera 의 감지 판정(거리·각도·시야)을 로그와 디버그 라인으로 보여준다."),
	ECVF_Cheat);
#endif

ASecurityCamera::ASecurityCamera()
{
	// 회전 연출·감지 판정 둘 다 매 프레임 값이 필요하다 — Hazards 중 Tick 을 쓰는 유일한 클래스다
	PrimaryActorTick.bCanEverTick = true;

	// bDisabled·bAlarmed 를 복제하려면 복제 액터여야 한다
	bReplicates = true;

	CameraBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraBase"));
	SetRootComponent(CameraBase);
	CameraBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CameraHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraHead"));
	CameraHead->SetupAttachment(CameraBase);
	CameraHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	VisionCone = CreateDefaultSubobject<USpotLightComponent>(TEXT("VisionCone"));
	VisionCone->SetupAttachment(CameraHead);
	VisionCone->SetCastShadows(false);
	VisionCone->SetIntensity(3000.f);
	VisionCone->SetLightColor(FLinearColor::White);

	// CameraHead 를 따라 매 틱 회전해야 하므로 반드시 Movable 이어야 한다 —
	// 라이트 컴포넌트는 베이킹을 노려 Static 으로 두는 경우가 흔해서 명시적으로 못박는다
	VisionCone->SetMobility(EComponentMobility::Movable);
}

void ASecurityCamera::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASecurityCamera, bDisabled);
	DOREPLIFETIME(ASecurityCamera, bAlarmed);
}

void ASecurityCamera::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SyncVisionConeToDetectionParams();
}

void ASecurityCamera::SyncVisionConeToDetectionParams()
{
	if (!IsValid(VisionCone))
	{
		return;
	}

	// 판정값과 어긋나면 "보이는 콘 밖인데 걸린다/보이는 콘 안인데 안 걸린다"가 생긴다 —
	// 그래서 BP 에서 따로 입력받지 않고 이 두 값에서 항상 새로 계산한다 (헤더 주석 참고)
	VisionCone->SetOuterConeAngle(DetectionHalfAngleDegrees);
	VisionCone->SetInnerConeAngle(DetectionHalfAngleDegrees * 0.5f);
	VisionCone->SetAttenuationRadius(DetectionRange);
}

void ASecurityCamera::BeginPlay()
{
	Super::BeginPlay();

	// 설정 실수는 "지나갔는데 아무 일도 안 일어난다" 하나로만 드러난다 (다른 Hazard 클래스와 동일 사유)
	if (!IsValid(CameraHead) || !CameraHead->GetStaticMesh())
	{
		UE_LOG(LogHazard, Warning,
			TEXT("[SecurityCamera:%s] CameraHead 에 메시가 없다 — 눈에 안 보이는 카메라가 된다"), *GetName());
	}

	// BP 에서 CameraBase 를 원하는 정면으로 미리 돌려 두면 그 값을 기준으로 좌우로 훑는다.
	// CameraHead 는 CameraBase 에 고정 부착만 돼 있고 따로 움직이지 않는다 — 몸체 전체가
	// 한 덩어리로 팬(pan)하는 쪽이 렌즈만 도는 것보다 자연스럽다는 판단에 따른 것이다.
	BaseBodyRotation = CameraBase->GetRelativeRotation();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(DetectionTimer, this, &ASecurityCamera::CheckDetection,
			DetectionCheckInterval, true);
	}
}

void ASecurityCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsValid(CameraBase))
	{
		return;
	}

	// 무력화 중(bDisabled)이면 정면에 고정한다 — 회전이 멈추는 것 자체가 "지금 안 통한다" 는
	// 시각 피드백이다. 경보 유지 중(bAlarmed)이면 포착한 각도 그대로 멈춰 선다 — 실제 CCTV가
	// 대상을 포착한 뒤 잠깐 멈칫하는 느낌을 준다. 둘 다 아니면 평소처럼 스윕한다.
	//
	// CameraBase(루트) 를 돌리면 그 자식인 CameraHead·VisionCone 도 통째로 같이 돈다 —
	// CheckDetection() 은 CameraHead 의 월드 트랜스폼을 매번 새로 읽으므로 감지 로직은 그대로 맞는다
	if (bDisabled)
	{
		CameraBase->SetRelativeRotation(BaseBodyRotation);
	}
	else if (!bAlarmed)
	{
		FRotator NewRotation = BaseBodyRotation;
		NewRotation.Yaw += ComputeSweepYawOffset();
		CameraBase->SetRelativeRotation(NewRotation);
	}

	// 무력화 중엔 시야도 꺼서 "지금 안 통한다" 는 걸 확실히 보여준다
	if (IsValid(VisionCone))
	{
		VisionCone->SetVisibility(!bDisabled);
	}
}

float ASecurityCamera::ComputeSweepYawOffset() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}

	const AGameStateBase* GameState = World->GetGameState();

	// GameState 가 아직 없는 BeginPlay 직후 극초반 프레임엔 로컬 시간으로 대체한다 —
	// 스윕 주기(초 단위) 대비 그 몇 프레임의 위상 오차는 무시할 수준이다
	const float Now = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();

	const float Phase = FMath::Sin(Now * (2.f * PI) / SweepPeriod);
	return Phase * SweepAngleDegrees;
}

void ASecurityCamera::CheckDetection()
{
	// 서버 권위 판정. 무력화 중(bDisabled)이면 훑지 않는다. bAlarmed 여도 계속 훑는다 —
	// 대상이 아직도 보이는지 확인해서 경보를 계속 유지(타이머 연장)할지, 놓쳐서 풀어줄지
	// 결정해야 하기 때문이다. 사운드·경계도·연출 훅은 "새로 포착한 순간"(아래 bAlarmed==false
	// 분기)에만 한 번 나간다 — 계속 보이는 동안은 반복 재생되지 않는다.
	if (!HasAuthority() || bDisabled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !IsValid(CameraHead))
	{
		return;
	}

	const FVector EyeLocation = CameraHead->GetComponentLocation();
	const FVector ForwardDir = CameraHead->GetForwardVector();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(DetectionHalfAngleDegrees));

#if !UE_BUILD_SHIPPING
	const bool bDebug = CVarHazardCameraDebug.GetValueOnGameThread();
#endif

	for (TActorIterator<ABaseCharacter> It(World); It; ++It)
	{
		ABaseCharacter* Target = *It;
		if (!IsValid(Target))
		{
			continue;
		}

		const FVector ToTarget = Target->GetActorLocation() - EyeLocation;
		const float Distance = ToTarget.Size();
		const bool bInRange = Distance > KINDA_SMALL_NUMBER && Distance <= DetectionRange;
		const float DotToTarget = bInRange ? FVector::DotProduct(ForwardDir, ToTarget / Distance) : -1.f;
		const bool bInCone = bInRange && DotToTarget >= CosHalfAngle;

#if !UE_BUILD_SHIPPING
		if (bDebug)
		{
			UE_LOG(LogHazard, Log,
				TEXT("[SecurityCamera:%s] 디버그 — %s 거리 %.0f/%.0f(범위:%s) dot %.2f/%.2f(콘 안:%s)"),
				*GetName(), *Target->GetName(), Distance, DetectionRange, bInRange ? TEXT("Y") : TEXT("N"),
				DotToTarget, CosHalfAngle, bInCone ? TEXT("Y") : TEXT("N"));
			DrawDebugLine(World, EyeLocation, Target->GetActorLocation(),
				bInCone ? FColor::Yellow : FColor::Silver, false, DetectionCheckInterval, 0, 1.5f);
		}
#endif

		if (!bInCone)
		{
			// 범위 밖이거나 콘 밖이다
			continue;
		}

		// 그림자 이동 중엔 무시한다 — State.ini 의 State.ShadowStep 코멘트("압력판 무시")를
		// 감지 장치 전체로 동일 적용한다(AHazardBase::IsValidHazardTarget 참고).
		// Target 은 이미 TActorIterator<ABaseCharacter> 가 준 타입이라 캐스팅 결과는 항상
		// 자기 자신이고, 여기서는 순수하게 ShadowStep 판정만 재사용하는 것이다.
		ABaseCharacter* ShadowCheckTarget = nullptr;
		if (!IsValidHazardTarget(Target, ShadowCheckTarget))
		{
			continue;
		}

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(Target);
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLocation, Target->GetActorLocation(),
			ECC_Visibility, Params);

#if !UE_BUILD_SHIPPING
		if (bDebug)
		{
			DrawDebugLine(World, EyeLocation, Target->GetActorLocation(),
				bBlocked ? FColor::Red : FColor::Green, false, DetectionCheckInterval, 0, 2.5f);
			if (bBlocked)
			{
				UE_LOG(LogHazard, Log, TEXT("[SecurityCamera:%s] 디버그 — %s 콘 안이지만 시야 차단 (%s 에 막힘)"),
					*GetName(), *Target->GetName(),
					Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("알 수 없음"));
			}
		}
#endif

		if (bBlocked)
		{
			// 벽 등에 가려 안 보인다
			continue;
		}

		// 지금 이 순간에도 보인다. 새로 포착한 순간(bAlarmed 가 꺼져 있던 상태)에만
		// 사운드·경계도·연출 훅을 한 번 내보낸다 — 계속 보이는 동안 매번 반복되지 않는다.
		if (!bAlarmed)
		{
			// 서버에서 직접 대입하면 RepNotify 가 안 불리므로 손으로도 불러야
			// 호스트 화면에도 바로 반영된다 (ABreakableWall 과 동일 사유)
			bAlarmed = true;
			OnRep_bAlarmed();

			// 세계 경계도를 직접 올린다. ALaserTrap/APressurePlate 와 동일 사유 —
			// Noise.ini 에 맞는 태그가 없어 SetAlertGauge01() 로 우회한다
			if (UAlertComponent* Alert = UAlertComponent::Get(this))
			{
				Alert->SetAlertGauge01(FMath::Clamp(Alert->GetAlertGauge01() + AlertGaugeIncrease, 0.f, 1.f));
			}

			UE_LOG(LogHazard, Log, TEXT("[SecurityCamera:%s] %s 를 발견했다 — 경보"),
				*GetName(), *Target->GetName());
		}
		else if (ContinuousAlertGaugeRate > 0.f)
		{
			// 이미 잡혀있는 상태로 또 확인된 것 — 최초 발각 몫과 별개로, 노출 시간에 비례해
			// 조금씩 더 올린다(기획서 3장 "대형 금고 절단 +3%/초"와 동일한 성격의 지속 압박)
			if (UAlertComponent* Alert = UAlertComponent::Get(this))
			{
				Alert->SetAlertGauge01(FMath::Clamp(
					Alert->GetAlertGauge01() + ContinuousAlertGaugeRate * DetectionCheckInterval, 0.f, 1.f));
			}
		}

		// 계속 보이는 한 타이머를 매번 새로 걸어 경보를 연장한다 — 시야에서 벗어나
		// MinRetriggerInterval 동안 다시 걸리지 않아야만 ClearAlarm() 이 실행돼 스윕으로 돌아간다
		GetWorldTimerManager().SetTimer(AlarmTimer, this, &ASecurityCamera::ClearAlarm, MinRetriggerInterval, false);

		// 같은 프레임에 여럿이 걸려도 하나만 처리하고 끝낸다
		break;
	}
}

void ASecurityCamera::ClearAlarm()
{
	bAlarmed = false;
	OnRep_bAlarmed();
}

void ASecurityCamera::OnRep_bAlarmed()
{
	if (bAlarmed)
	{
		// 판정은 이미 끝났다. 경고등 연출 등은 BP 몫이다 (헤더 주석 참고)
		OnPlayerDetected();
		StartAlarmSound();
	}
	else
	{
		OnAlarmCleared();
		StopAlarmSound();
	}
}

void ASecurityCamera::StartAlarmSound()
{
	const UWorld* World = GetWorld();

	// 데디케이티드 서버는 화면도 스피커도 없다 (다른 Hazard 클래스들과 동일 사유)
	if (!World || World->GetNetMode() == NM_DedicatedServer || !IsValid(AlarmSound))
	{
		return;
	}

	// 이미 재생 중이면 다시 만들지 않는다 — 인스턴스가 두 개 겹쳐 재생되는 것을 막는다
	if (IsValid(AlarmAudioComponent) && AlarmAudioComponent->IsPlaying())
	{
		return;
	}

	AlarmAudioComponent = UGameplayStatics::SpawnSoundAtLocation(World, AlarmSound, GetActorLocation());
}

void ASecurityCamera::StopAlarmSound()
{
	if (IsValid(AlarmAudioComponent))
	{
		AlarmAudioComponent->Stop();
	}
	AlarmAudioComponent = nullptr;
}

void ASecurityCamera::Disable(float Duration)
{
	// 여러 시스템(EMP 등)이 부를 수 있는 API 라 게이트를 안에 둔다 (CLAUDE.md 3절 규칙)
	if (!HasAuthority() || Duration <= 0.f)
	{
		return;
	}

	bDisabled = true;

	GetWorldTimerManager().SetTimer(DisableTimer, this, &ASecurityCamera::ReEnable, Duration, false);
}

void ASecurityCamera::ReEnable()
{
	bDisabled = false;
}
