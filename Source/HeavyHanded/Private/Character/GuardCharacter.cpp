#include "Character/GuardCharacter.h"
#include "AI/GuardAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "AI/GuardTypes.h"

//#include "Noise/PerceptionMeterComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Noise/PerceptionMeterComponent.h"

#include "UI/DetectionGaugeWidget.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"
#include "BehaviorTree/BlackboardComponent.h"
//#include "AI/GuardSightAComponent.h"

#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"


AGuardCharacter::AGuardCharacter()
{
	bReplicates = true;
	//???? < AI 컨트롤러에도 있음 > 다시 이쪽으로 옮김
	PerceptionMeterComponent = CreateDefaultSubobject<UPerceptionMeterComponent>(TEXT("PerceptionMeter"));

	// 팀 어피니에이션(GuardAIController::SetGenericTeamId)으로 서로를 "감지"는 안 하게 됐지만,
	// 순찰 경로가 겹치면 캡슐끼리 물리적으로 계속 밀며 그 자리에 멈춰(마주보는 것처럼 보임) 있고,
	// 그 상태에서는 PatrolArrivalRadius 안으로 못 들어와 다음 순찰 지점으로도 못 넘어간다.
	// RVO Avoidance를 켜서 서로를 스쳐 지나가도록 미리 피하게 한다.
	GetCharacterMovement()->bUseRVOAvoidance = true;



	// Controller의 Yaw를 캐릭터 회전에 반영
	bUseControllerRotationYaw = true;

	// 이동 방향으로 캐릭터가 자동 회전하지 않음
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// AIController가 원하는 회전 방향을 캐릭터가 따라감
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	// 서버 AI Perception과 EyeSocket 읽기가 두리번 애니메이션의 최신 본 포즈를 보게 한다.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;




	// 위젯 클래스는 여기서 강제하지 않는다 - BP_GuardBase 등 파생 BP에서
	// 컴포넌트 디테일 패널의 Widget Class로 WBP_DetectionGauge를 지정할 것.

	// Screen space로 두면 항상 카메라를 향해 평면으로 그려지므로(빌보드), World space처럼
	// 경비가 돌아설 때 게이지가 옆으로 눕는 문제가 없다. 위치만 머리 위로 올려서 붙인다.
	DetectionGaugeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DetectionGaugeWidgetComponent"));
	DetectionGaugeWidgetComponent->SetupAttachment(GetCapsuleComponent());
	DetectionGaugeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // 나중에 다시 고칠 것
	DetectionGaugeWidgetComponent->SetDrawSize(FVector2D(120.f, 40.f));
	DetectionGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));


	// 기존 BP 컴포넌트 참조를 유지한다. 청각 게이지 표시는 통합 위젯이 담당한다.
	HearingGaugeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NoiseGaugeWidgetComponent"));
	HearingGaugeWidgetComponent->SetupAttachment(GetCapsuleComponent());
	HearingGaugeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // 나중에 다시 고칠 것
	HearingGaugeWidgetComponent->SetDrawSize(FVector2D(120.f, 16.f));
	HearingGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 135.f));
	HearingGaugeWidgetComponent->SetVisibility(false);
	// HearingGaugeWidgetComponent->SetDrawAtDesiredSize(false);


	SightDebugMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SightDebugMesh"));
	SightDebugMesh->SetupAttachment(GetRootComponent());
	SightDebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SightDebugMesh->SetCastShadow(false);
	SightDebugMesh->SetVisibility(false);
	SightDebugMesh->SetHiddenInGame(true);

}

FTransform AGuardCharacter::GetEyeSocketTransform() const
{
	static const FName EyeSocketName(TEXT("EyeSocket"));
	const USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (bUseEyeSocketForSight && IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(EyeSocketName))
	{
		return CharacterMesh->GetSocketTransform(EyeSocketName, RTS_World);
	}

	return FTransform(GetActorRotation(), GetActorLocation() + FVector(0.0f, 0.0f, EyeHeight));
}

FRotator AGuardCharacter::GetSightSocketRotation() const
{
	static const FName EyeSocketName(TEXT("EyeSocket"));
	const USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (bUseEyeSocketForSight && IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(EyeSocketName))
	{
		// EyeSocket을 사용하는 경우에만 소켓의 정면 축을 보정한다.
		FRotator SightRotation = CharacterMesh->GetSocketTransform(EyeSocketName, RTS_World).Rotator();
		SightRotation.Yaw += 90.0f;
		return SightRotation;
	}

#if WITH_EDITORONLY_DATA
	const UArrowComponent* CharacterArrow = GetArrowComponent();
	if (IsValid(CharacterArrow))
	{
		return CharacterArrow->GetComponentRotation();
	}
#endif

	// 기본 화살표가 제외되는 패키징에서는 캡슐의 정면 방향을 사용한다.
	const UCapsuleComponent* CharacterCapsule = GetCapsuleComponent();
	return IsValid(CharacterCapsule) ? CharacterCapsule->GetComponentRotation() : GetActorRotation();
}

void AGuardCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const FTransform EyeTransform = GetEyeSocketTransform();
	OutLocation = EyeTransform.GetLocation();
	OutRotation = GetSightSocketRotation();
}

void AGuardCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGuardCharacter, ReplicatedSightRadius);
	DOREPLIFETIME(AGuardCharacter, ReplicatedSightHalfAngle);
	DOREPLIFETIME(AGuardCharacter, bReplicatedDrawSightDebug);
	DOREPLIFETIME(AGuardCharacter, ReplicatedSightDebugRotation);
	DOREPLIFETIME(AGuardCharacter, ReplicatedDetectionGaugePercent);
	DOREPLIFETIME(AGuardCharacter, ReplicatedAggroTarget);
	DOREPLIFETIME(AGuardCharacter, ReplicatedLookAroundType);
	DOREPLIFETIME(AGuardCharacter, bReplicatedIsArresting);
	DOREPLIFETIME(AGuardCharacter, SightDebugUpdateInterval);
	DOREPLIFETIME(AGuardCharacter, bSightDebugChasing);
	DOREPLIFETIME(AGuardCharacter, SightDebugFanColor);
	DOREPLIFETIME(AGuardCharacter, SightDebugMaterial);
}

void AGuardCharacter::SetSightDebugAppearance(bool bInChasing, FLinearColor InColor, UMaterialInterface* InMaterial)
{
	if (!HasAuthority())
	{
		return;
	}
	bSightDebugChasing = bInChasing;
	SightDebugFanColor = InColor;
	SightDebugMaterial = InMaterial;
}

void AGuardCharacter::UpdateLocalSightDebugTimer()
{
	if (!IsValid(GetWorld()))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(LocalSightDebugTimerHandle);
	if (GetNetMode() == NM_DedicatedServer || !IsSightMeshVisible())
	{
		return;
	}
	const float Interval = FMath::Max(SightDebugUpdateInterval, 0.05f);
	// 여러 경비가 같은 프레임에 지면 트레이스를 몰아서 실행하지 않도록 분산한다.
	const float InitialDelay = Interval * (1.f + static_cast<float>(GetUniqueID() % 32) / 32.f);
	GetWorldTimerManager().SetTimer(LocalSightDebugTimerHandle, this, &AGuardCharacter::DrawLocalSightDebugMesh, Interval, true, InitialDelay);
}

void AGuardCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LocalSightDebugTimerHandle);
	GetWorldTimerManager().ClearTimer(HeadGaugeUpdateTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AGuardCharacter::SetArresting(bool bNewArresting)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bReplicatedIsArresting != bNewArresting)
	{
		bReplicatedIsArresting = bNewArresting;
		ForceNetUpdate();
	}
	OnRep_Arresting();
}

void AGuardCharacter::OnRep_Arresting()
{
	if (IsValid(GetMesh()))
	{
		if (UGuardAnimInstance* AnimInstance = Cast<UGuardAnimInstance>(GetMesh()->GetAnimInstance()); IsValid(AnimInstance))
		{
			AnimInstance->SetArresting(bReplicatedIsArresting);
		}
	}
}

void AGuardCharacter::SetLookAroundType(EGuardLookAroundType NewType)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ReplicatedLookAroundType != NewType)
	{
		ReplicatedLookAroundType = NewType;
		ForceNetUpdate();
	}
	OnRep_LookAroundType();
}

void AGuardCharacter::OnRep_LookAroundType()
{
	if (IsValid(GetMesh()))
	{
		if (UGuardAnimInstance* AnimInstance = Cast<UGuardAnimInstance>(GetMesh()->GetAnimInstance()); IsValid(AnimInstance))
		{
			AnimInstance->SetLookAroundType(ReplicatedLookAroundType);
		}
	}
}

void AGuardCharacter::SetAggravationMontage(UAnimMontage* Montage, bool bPlay, float BlendOutTime)
{
	if (!HasAuthority() || !IsValid(Montage))
	{
		return;
	}
	Multicast_SetAggravationMontage(Montage, bPlay, BlendOutTime);
}

void AGuardCharacter::Multicast_SetAggravationMontage_Implementation(UAnimMontage* Montage, bool bPlay, float BlendOutTime)
{
	if (!IsValid(Montage) || !IsValid(GetMesh()))
	{
		return;
	}
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		return;
	}
	if (bPlay)
	{
		AnimInstance->Montage_Play(Montage);
	}
	else if (AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(FMath::Max(0.f, BlendOutTime), Montage);
	}
}

void AGuardCharacter::OnRep_SightDebugState()
{
	if (IsValid(SightDebugMesh))
	{
		SightDebugMesh->SetVisibility(IsSightMeshVisible());
		SightDebugMesh->SetHiddenInGame(!IsSightMeshVisible());
	}
	UpdateLocalSightDebugTimer();

}

void AGuardCharacter::OnRep_SightDebugRotation()
{
	if (IsValid(SightDebugMesh))
	{
		SightDebugMesh->SetWorldRotation(ReplicatedSightDebugRotation);
	}
}

void AGuardCharacter::SetReplicatedSightDebugState(float InSightRadius, float InSightHalfAngle)
{
	if (!HasAuthority())
	{
		return;
	}

	InitializeSightDebugDisplay();
	ReplicatedSightRadius = InSightRadius;
	ReplicatedSightHalfAngle = InSightHalfAngle;
	OnRep_SightDebugState();
	ForceNetUpdate();
}

void AGuardCharacter::SetReplicatedSightDebugRotation(FRotator InRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedSightDebugRotation = InRotation;
	if (IsValid(SightDebugMesh))
	{
		SightDebugMesh->SetWorldRotation(ReplicatedSightDebugRotation);
	}
}

void AGuardCharacter::SetReplicatedDetectionGauge(float InGaugePercent)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedDetectionGaugePercent = FMath::Clamp(InGaugePercent, 0.0f, 100.0f);
}

void AGuardCharacter::DrawLocalSightDebugMesh()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(SightDebugMesh) || !IsValid(GetCapsuleComponent()) || !IsSightMeshVisible() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float SightRadius = ReplicatedSightRadius;
	const float HalfAngle = ReplicatedSightHalfAngle;
	if (SightRadius <= 0.f || HalfAngle <= 0.f)
	{
		SightDebugMesh->ClearAllMeshSections();
		return;
	}
	constexpr int32 ArcSegments = 96;
	const int32 AngularPointCount = ArcSegments + 1;
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector LocalOrigin(0.f, 0.f, -CapsuleHalfHeight + 2.f);
	const FRotator SightMeshRotation(0.f, ReplicatedSightDebugRotation.Yaw, 0.f);
	SightDebugMesh->SetWorldRotation(SightMeshRotation);
	const FTransform MeshTransform = SightDebugMesh->GetComponentTransform();
	const FVector WorldOrigin = MeshTransform.TransformPosition(LocalOrigin);
	const FVector UpDirection = FVector::UpVector;
	constexpr float MinGroundNormalZ = 0.7f;
	const FVector EyeLocation(WorldOrigin.X, WorldOrigin.Y, GetRootComponent()->GetComponentLocation().Z + GetEyeHeight());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSightDebug), true, this);
	const bool bIsChasing = bSightDebugChasing;
	if (bIsChasing && IsValid(ReplicatedAggroTarget))
	{
		Params.AddIgnoredActor(ReplicatedAggroTarget);
	}

	// 눈높이보다 낮은 길과 턱은 건너뛰고, 눈높이에 닿는 장애물만 찾는다.
	TArray<float> VisibleDistances;
	VisibleDistances.Reserve(AngularPointCount);

	for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
	{
		const float Alpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
		const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, Alpha);
		const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
		const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
		const FVector TraceEnd = WorldOrigin + WorldDirection * SightRadius;
		FCollisionQueryParams AngleParams = Params;
		float VisibleDistance = SightRadius;

		// 눈높이보다 낮은 장애물은 건너뛰고, 눈높이에 닿는 장애물에서만 자른다.
		while (true)
		{
			FHitResult Hit;
			const bool bBlocked = World->LineTraceSingleByChannel(
				Hit, WorldOrigin, TraceEnd, ECC_Visibility, AngleParams);
			if (!bBlocked)
			{
				break;
			}

			const float HitDistance = FMath::Clamp(
				FVector::DotProduct(Hit.Location - WorldOrigin, WorldDirection), 0.0f, SightRadius);
			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!IsValid(HitComponent))
			{
				VisibleDistance = HitDistance;
				break;
			}

			if (Hit.ImpactNormal.Z >= MinGroundNormalZ)
			{
				AngleParams.AddIgnoredComponent(HitComponent);
				continue;
			}

			const FVector BoundsOrigin = HitComponent->Bounds.Origin;
			const FVector BoundsExtent = HitComponent->Bounds.BoxExtent;
			const float ObstacleTopZ = BoundsOrigin.Z + BoundsExtent.Z;

			// 눈높이보다 낮은 길과 턱은 부채꼴을 자르지 않는다.
			// 눈높이에 닿는 벽과 건물부터 시야 외곽을 자른다.
			const bool bBlocksSightFan = ObstacleTopZ >= EyeLocation.Z;

			if (bBlocksSightFan)
			{
				VisibleDistance = HitDistance;
				if (Hit.GetActor() && Hit.GetActor()->IsA<ACharacter>())
				{
					VisibleDistance = FMath::Min(VisibleDistance + 50.0f, SightRadius);
				}
				break;
			}

			AngleParams.AddIgnoredComponent(HitComponent);
		}

		VisibleDistances.Add(VisibleDistance);
	}



	// 평면 부채꼴은 발밑보다 조금 위에 두어 항상 깔끔한 형태를 유지한다.
	constexpr float FlatMeshHeightOffset = 10.0f;
	const FVector FlatMeshOrigin(0.0f, 0.0f, -CapsuleHalfHeight + FlatMeshHeightOffset);

	TArray<FVector> FlatVertices;
	TArray<int32> FlatTriangles;
	TArray<FVector> FlatNormals;
	TArray<FVector2D> FlatUV0;
	TArray<FLinearColor> FlatVertexColors;
	TArray<FProcMeshTangent> FlatTangents;
	FlatVertices.Reserve(AngularPointCount + 1);
	FlatTriangles.Reserve(ArcSegments * 3);
	FlatNormals.Reserve(AngularPointCount + 1);
	FlatUV0.Reserve(AngularPointCount + 1);
	FlatVertexColors.Reserve(AngularPointCount + 1);
	const FLinearColor FlatFanColor = SightDebugFanColor;

	FlatVertices.Add(FlatMeshOrigin);
	FlatNormals.Add(FVector::UpVector);
	FlatUV0.Add(FVector2D::ZeroVector);
	FlatVertexColors.Add(FlatFanColor);

	for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
	{
		const float AngleAlpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
		const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, AngleAlpha);
		const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
		FlatVertices.Add(FlatMeshOrigin + LocalDirection * VisibleDistances[AngleIndex]);
		FlatNormals.Add(FVector::UpVector);
		FlatUV0.Add(FVector2D::ZeroVector);
		FlatVertexColors.Add(FlatFanColor);
	}

	for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
	{
		FlatTriangles.Add(0);
		FlatTriangles.Add(AngleIndex + 2);
		FlatTriangles.Add(AngleIndex + 1);
	}

	TArray<FVector> GroundVertices;
	TArray<int32> GroundTriangles;
	TArray<FVector> GroundNormals;
	TArray<FVector2D> GroundUV0;
	TArray<FLinearColor> GroundVertexColors;
	TArray<FProcMeshTangent> GroundTangents;
	bool bHasGroundOverlay = false;

	if (!bIsChasing)
	{
		// 낮은 길의 윗면은 평면 부채꼴과 별도 메시로 만든다.
		// 서로 다른 높이의 바닥을 연결하지 않아 경사 삼각형이 생기지 않는다.
		constexpr float GroundTraceUp = 1000.0f;
		constexpr float GroundTraceDown = 5000.0f;
		constexpr float GroundSurfaceOffset = 2.0f;
		// 길 표면을 더 촘촘히 샘플링해 큰 삼각형 조각을 줄인다.
		constexpr float GroundSampleSpacing = 100.0f;
		// 같은 평면에 가까운 정점만 연결한다.
		// 낮은 길의 경계는 끊기므로 바닥으로 내려가는 경사 삼각형이 생기지 않는다.
		constexpr float FlatSurfaceNormalZ = 0.98f;
		constexpr float MaxOverlayHeightDelta = 5.0f;
		const int32 GroundRadialSegments = FMath::Clamp(FMath::CeilToInt(SightRadius / GroundSampleSpacing), 4, 24);
		const FVector FlatMeshOriginWorld = MeshTransform.TransformPosition(FlatMeshOrigin);

		struct FGroundOverlaySample
		{
			FVector SampleWorldPoint = FVector::ZeroVector;
			FVector SurfaceWorldPoint = FVector::ZeroVector;
			FVector SurfaceWorldNormal = FVector::UpVector;
			float SurfaceHeight = 0.0f;
			bool bIsValid = false;
		};

		TArray<FGroundOverlaySample> GroundOverlaySamples;
		const int32 GroundVertexCount = (GroundRadialSegments + 1) * AngularPointCount;
		GroundOverlaySamples.Reserve(GroundVertexCount);
		const int32 GroundCellCount = GroundRadialSegments * ArcSegments;
		GroundVertices.Reserve(GroundCellCount * 8);
		GroundNormals.Reserve(GroundCellCount * 8);
		GroundUV0.Reserve(GroundCellCount * 8);
		GroundVertexColors.Reserve(GroundCellCount * 8);
		GroundTriangles.Reserve(GroundCellCount * 12);

		auto QueryGroundOverlaySample = [&](const FVector& SampleWorldPoint)
		{
			FGroundOverlaySample Sample;
			Sample.SampleWorldPoint = SampleWorldPoint;
			const FVector TraceStart = SampleWorldPoint + UpDirection * GroundTraceUp;
			const FVector TraceEnd = SampleWorldPoint - UpDirection * GroundTraceDown;
			FHitResult GroundHit;
			const bool bGroundTraceHit = World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params);
			const AActor* GroundActor = GroundHit.GetActor();
			const bool bIsWalkableGround = bGroundTraceHit && GroundHit.ImpactNormal.Z >= MinGroundNormalZ &&
				(!IsValid(GroundActor) || !GroundActor->IsA<ACharacter>());
			const bool bIsFlatSurface = bIsWalkableGround && GroundHit.ImpactNormal.Z >= FlatSurfaceNormalZ;
			const bool bIsAboveFlatMesh = bIsWalkableGround &&
				GroundHit.ImpactPoint.Z > FlatMeshOriginWorld.Z + GroundSurfaceOffset;
			const bool bIsBelowEyeHeight = bIsWalkableGround && GroundHit.ImpactPoint.Z < EyeLocation.Z;
			Sample.bIsValid = bIsFlatSurface && bIsAboveFlatMesh && bIsBelowEyeHeight;
			if (Sample.bIsValid)
			{
				Sample.SurfaceWorldPoint = GroundHit.ImpactPoint + UpDirection * GroundSurfaceOffset;
				Sample.SurfaceWorldNormal = GroundHit.ImpactNormal;
				Sample.SurfaceHeight = GroundHit.ImpactPoint.Z;
			}
			return Sample;
		};

		for (int32 RadialIndex = 0; RadialIndex <= GroundRadialSegments; ++RadialIndex)
		{
			const float RadialAlpha = static_cast<float>(RadialIndex) / static_cast<float>(GroundRadialSegments);
			for (int32 AngleIndex = 0; AngleIndex <= ArcSegments; ++AngleIndex)
			{
				const float AngleAlpha = static_cast<float>(AngleIndex) / static_cast<float>(ArcSegments);
				const float AngleDegrees = FMath::Lerp(-HalfAngle, HalfAngle, AngleAlpha);
				const FVector LocalDirection = FVector::ForwardVector.RotateAngleAxis(AngleDegrees, UpDirection);
				const FVector WorldDirection = MeshTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
				const float Distance = VisibleDistances[AngleIndex] * RadialAlpha;
				const FVector SampleWorldPoint = WorldOrigin + WorldDirection * Distance;
				GroundOverlaySamples.Add(QueryGroundOverlaySample(SampleWorldPoint));
			}
		}

		// 삼각형 단위로 유효한 지면 경계를 찾아 잘라낸다.
		TArray<FGroundOverlaySample> ClippedTriangle;
		ClippedTriangle.Reserve(4);
		TMap<uint64, FGroundOverlaySample> BoundarySampleCache;
		BoundarySampleCache.Reserve(GroundCellCount * 2);
		auto AddClippedGroundTriangle = [&](int32 FirstIndex, int32 SecondIndex, int32 ThirdIndex)
		{
			const int32 TriangleIndices[] = { FirstIndex, SecondIndex, ThirdIndex };
			ClippedTriangle.Reset();
			for (int32 EdgeIndex = 0; EdgeIndex < UE_ARRAY_COUNT(TriangleIndices); ++EdgeIndex)
			{
				const int32 EdgeStartIndex = TriangleIndices[EdgeIndex];
				const int32 EdgeEndIndex = TriangleIndices[(EdgeIndex + 1) % UE_ARRAY_COUNT(TriangleIndices)];
				const FGroundOverlaySample& EdgeStart = GroundOverlaySamples[EdgeStartIndex];
				const FGroundOverlaySample& EdgeEnd = GroundOverlaySamples[EdgeEndIndex];

				if (EdgeStart.bIsValid)
				{
					ClippedTriangle.Add(EdgeStart);
				}

				if (EdgeStart.bIsValid == EdgeEnd.bIsValid)
				{
					continue;
				}

				const uint64 BoundaryCacheKey =
					(static_cast<uint64>(FMath::Min(EdgeStartIndex, EdgeEndIndex)) << 32) |
					static_cast<uint64>(FMath::Max(EdgeStartIndex, EdgeEndIndex));
				if (const FGroundOverlaySample* CachedSample = BoundarySampleCache.Find(BoundaryCacheKey))
				{
					ClippedTriangle.Add(*CachedSample);
					continue;
				}

				float ValidAlpha = EdgeStart.bIsValid ? 0.0f : 1.0f;
				float InvalidAlpha = EdgeStart.bIsValid ? 1.0f : 0.0f;
				FGroundOverlaySample BoundarySample = EdgeStart.bIsValid ? EdgeStart : EdgeEnd;
				for (int32 SubdivisionIndex = 0; SubdivisionIndex < 8; ++SubdivisionIndex)
				{
					const float TestAlpha = (ValidAlpha + InvalidAlpha) * 0.5f;
					const FVector TestWorldPoint = FMath::Lerp(
						EdgeStart.SampleWorldPoint, EdgeEnd.SampleWorldPoint, TestAlpha);
					const FGroundOverlaySample TestSample = QueryGroundOverlaySample(TestWorldPoint);
					if (TestSample.bIsValid)
					{
						ValidAlpha = TestAlpha;
						BoundarySample = TestSample;
					}
					else
					{
						InvalidAlpha = TestAlpha;
					}
				}
				BoundarySampleCache.Add(BoundaryCacheKey, BoundarySample);
				ClippedTriangle.Add(BoundarySample);
			}

			if (ClippedTriangle.Num() < 3)
			{
				return;
			}

			float MinHeight = ClippedTriangle[0].SurfaceHeight;
			float MaxHeight = MinHeight;
			for (const FGroundOverlaySample& TriangleSample : ClippedTriangle)
			{
				MinHeight = FMath::Min(MinHeight, TriangleSample.SurfaceHeight);
				MaxHeight = FMath::Max(MaxHeight, TriangleSample.SurfaceHeight);
			}
			if (MaxHeight - MinHeight > MaxOverlayHeightDelta)
			{
				return;
			}

			const int32 TriangleVertexStart = GroundVertices.Num();
			for (const FGroundOverlaySample& TriangleSample : ClippedTriangle)
			{
				GroundVertices.Add(MeshTransform.InverseTransformPosition(TriangleSample.SurfaceWorldPoint));
				GroundNormals.Add(MeshTransform.InverseTransformVectorNoScale(TriangleSample.SurfaceWorldNormal).GetSafeNormal());
				GroundUV0.Add(FVector2D::ZeroVector);
				GroundVertexColors.Add(FlatFanColor);
			}

			for (int32 TriangleIndex = 1; TriangleIndex < ClippedTriangle.Num() - 1; ++TriangleIndex)
			{
				GroundTriangles.Add(TriangleVertexStart);
				GroundTriangles.Add(TriangleVertexStart + TriangleIndex);
				GroundTriangles.Add(TriangleVertexStart + TriangleIndex + 1);
			}
		};

		for (int32 RadialIndex = 1; RadialIndex <= GroundRadialSegments; ++RadialIndex)
		{
			const int32 InnerBase = (RadialIndex - 1) * AngularPointCount;
			const int32 OuterBase = RadialIndex * AngularPointCount;
			for (int32 AngleIndex = 0; AngleIndex < ArcSegments; ++AngleIndex)
			{
				const int32 InnerCurrent = InnerBase + AngleIndex;
				const int32 InnerNext = InnerCurrent + 1;
				const int32 OuterCurrent = OuterBase + AngleIndex;
				const int32 OuterNext = OuterCurrent + 1;
				AddClippedGroundTriangle(InnerCurrent, InnerNext, OuterNext);
				AddClippedGroundTriangle(OuterNext, OuterCurrent, InnerCurrent);
			}
		}

		bHasGroundOverlay = GroundTriangles.Num() > 0;
	}



	SightDebugMesh->ClearAllMeshSections();
	SightDebugMesh->CreateMeshSection_LinearColor(
		0,
		FlatVertices,
		FlatTriangles,
		FlatNormals,
		FlatUV0,
		FlatVertexColors,
		FlatTangents,
		false);

	if (bHasGroundOverlay)
	{
		SightDebugMesh->CreateMeshSection_LinearColor(
			1,
			GroundVertices,
			GroundTriangles,
			GroundNormals,
			GroundUV0,
			GroundVertexColors,
			GroundTangents,
			false);
	}

	SightDebugMesh->SetVisibility(true);
	SightDebugMesh->SetHiddenInGame(false);
	SightDebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (SightDebugMaterial)
	{
		SightDebugMesh->SetMaterial(0, SightDebugMaterial);
		if (bHasGroundOverlay)
		{
			SightDebugMesh->SetMaterial(1, SightDebugMaterial);
		}
	}

}

void AGuardCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 기존 BP에 저장된 진단 활성화 값도 시작 시 해제한다.
	bDrawPerceptionWidgetDebug = false;
	InitializeSightDebugDisplay();
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		// 파생 블루프린트의 Mesh 기본값이 생성자 설정을 덮을 수 있어 런타임에도 강제한다.
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		CharacterMesh->bEnableUpdateRateOptimizations = false;

		if (bUseEyeSocketForSight && !CharacterMesh->DoesSocketExist(TEXT("EyeSocket")))
		{
			UE_LOG(LogGuardAI, Warning, TEXT("[%s] EyeSocket을 찾지 못해 액터 방향을 시야 기준으로 사용합니다. Mesh=%s"),
				*GetNameSafe(this), *GetNameSafe(CharacterMesh->GetSkeletalMeshAsset()));
		}
	}
	SetHeadGaugeUpdateInterval(0.1f);
	UpdateLocalSightDebugTimer();

	UpdatePerceptionWidgets();


}

void AGuardCharacter::UpdatePerceptionWidgets()
{

	if (IsValid(DetectionGaugeWidgetComponent))
	{
		const bool bEnablePerception = bEnableSight || bEnableHearing;
		DetectionGaugeWidgetComponent->SetVisibility(bEnablePerception);
		bEnablePerception ? DetectionGaugeWidgetComponent->Activate() : DetectionGaugeWidgetComponent->Deactivate();
	}

	if (IsValid(HearingGaugeWidgetComponent))
	{
		HearingGaugeWidgetComponent->SetVisibility(false);
		HearingGaugeWidgetComponent->Deactivate();
	}

}

void AGuardCharacter::SetHeadGaugeUpdateInterval(float NewInterval)
{
	HeadGaugeUpdateInterval = NewInterval;

	GetWorldTimerManager().ClearTimer(HeadGaugeUpdateTimerHandle);
	GetWorldTimerManager().SetTimer(HeadGaugeUpdateTimerHandle,
		this, &AGuardCharacter::UpdateHeadGaugeWidget, HeadGaugeUpdateInterval, true);
}

void AGuardCharacter::StopHeadGaugeUpdate()
{
	GetWorldTimerManager().ClearTimer(HeadGaugeUpdateTimerHandle);
}

void AGuardCharacter::UpdateHeadGaugeWidget()
{
	const auto DrawWidgetDebug = [this](const FString& Message, const FColor& Color)
	{
		if (!bDrawPerceptionWidgetDebug)
		{
			return;
		}
		const FString FullMessage = FString::Printf(TEXT("[통합 게이지][%s] %s"), *GetName(), *Message);
		if (IsValid(GetWorld()))
		{
			const float Now = GetWorld()->GetTimeSeconds();
			if (Now - LastPerceptionWidgetDebugLogTime >= 1.f)
			{
				LastPerceptionWidgetDebugLogTime = Now;
				UE_LOG(LogGuardAI, Warning, TEXT("%s"), *FullMessage);
			}
		}
		if (bDrawPerceptionWidgetDebug && IsValid(GEngine) && GetNetMode() != NM_DedicatedServer)
		{
			// 경비별 고정 키로 같은 진단을 갱신한다. 순찰 실패 메시지와는 다른 키를 사용한다.
			const uint64 MessageKey = (static_cast<uint64>(GetUniqueID()) << 32) | 1;
			GEngine->AddOnScreenDebugMessage(MessageKey, FMath::Max(HeadGaugeUpdateInterval * 2.f, 1.f), Color, FullMessage);
		}
	};
	if (HasAuthority())
	{
		ReplicatedAggroTarget = nullptr;
		AGuardAIController* GuardAIController = Cast<AGuardAIController>(GetController());
		if (!IsValid(GuardAIController))
		{
			SetReplicatedDetectionGauge(0.0f);
		}
		else
		{
			const UBlackboardComponent* BlackboardComp = GuardAIController->GetBlackboardComponent();
			AActor* TargetActor = IsValid(BlackboardComp)
				? Cast<AActor>(BlackboardComp->GetValueAsObject(GuardAIKeys::TargetActor))
				: nullptr;
			const float GaugePercent = IsValid(TargetActor) ? GuardAIController->GetDetectionGaugePercent() : 0.0f;
			SetReplicatedDetectionGauge(GaugePercent);
			ReplicatedAggroTarget = IsValid(TargetActor) ? TargetActor : nullptr;
		}
	}

	if (!IsValid(DetectionGaugeWidgetComponent))
	{
		DrawWidgetDebug(TEXT("실패: 시야 위젯 컴포넌트 없음"), FColor::Red);
		return;
	}

	UDetectionGaugeWidget* GaugeWidget = Cast<UDetectionGaugeWidget>(DetectionGaugeWidgetComponent->GetUserWidgetObject());
	if (!GaugeWidget)
	{
		DrawWidgetDebug(FString::Printf(TEXT("연결 실패: 지정클래스=%s 실제위젯=%s / DetectionGaugeWidget 상속 및 지정 컴포넌트 확인"), *GetNameSafe(DetectionGaugeWidgetComponent->GetWidgetClass()), *GetNameSafe(DetectionGaugeWidgetComponent->GetUserWidgetObject())), FColor::Red);
		// Widget Class 가 아직 지정 안 됐거나(파생 BP에서 WBP_DetectionGauge 미설정),
		// 컴포넌트가 아직 위젯 인스턴스를 만들기 전(BeginPlay 타이밍)일 수 있다.
		return;
	}

	float HearingPercent = 0.f;
	if (IsValid(PerceptionMeterComponent))
	{
		// 기존 청각 위젯처럼 실제 인지 값을 조사 진입 임계값 기준으로 정규화한다.
		const float FullThreshold = PerceptionMeterComponent->GetPerceptionFullThreshold();
		HearingPercent = FullThreshold > 0.f ? FMath::Clamp(PerceptionMeterComponent->GetPerception01() / FullThreshold, 0.f, 1.f) * 100.f : 0.f;
	}
	GaugeWidget->SetPerceptionGaugePercents(ReplicatedDetectionGaugePercent, HearingPercent, bEnableSight, bEnableHearing);
	GaugeWidget->SetAggroTarget(ReplicatedAggroTarget);
	if (!bDrawPerceptionWidgetDebug)
	{
		return;
	}
	const FVector2D DrawSize = DetectionGaugeWidgetComponent->GetDrawSize();
	ULocalPlayer* LocalPlayer = DetectionGaugeWidgetComponent->GetOwnerPlayer();
	APlayerController* PlayerController = IsValid(LocalPlayer) ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	bool bProjected = false;
	if (IsValid(PlayerController))
	{
		bProjected = PlayerController->ProjectWorldLocationToScreen(DetectionGaugeWidgetComponent->GetComponentLocation(), ScreenPosition);
		PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	}
	const bool bInsideViewport = bProjected && ScreenPosition.X >= 0.f && ScreenPosition.Y >= 0.f && ScreenPosition.X < ViewportWidth && ScreenPosition.Y < ViewportHeight;
	const FString RegistrationInfo = FString::Printf(TEXT("공간=%s 로컬플레이어=%s 화면컨트롤러=%s 경비숨김=%s 컴포넌트틱=%s 투영=%s 화면내=%s 좌표=(%.0f,%.0f) 화면=%dx%d 위치=%s"), DetectionGaugeWidgetComponent->GetWidgetSpace() == EWidgetSpace::Screen ? TEXT("Screen") : TEXT("World"), *GetNameSafe(LocalPlayer), *GetNameSafe(PlayerController), IsHidden() ? TEXT("예") : TEXT("아니오"), DetectionGaugeWidgetComponent->IsComponentTickEnabled() ? TEXT("켜짐") : TEXT("꺼짐"), bProjected ? TEXT("성공") : TEXT("실패"), bInsideViewport ? TEXT("예") : TEXT("아니오"), ScreenPosition.X, ScreenPosition.Y, ViewportWidth, ViewportHeight, *DetectionGaugeWidgetComponent->GetComponentLocation().ToCompactString());
	DrawWidgetDebug(FString::Printf(TEXT("%s | 시야=%.0f 청각=%.0f 감각=%s/%s 컴포넌트표시=%s 게임중숨김=%s 출력크기=%.0fx%.0f | %s | %s"), *GetNameSafe(GaugeWidget->GetClass()), ReplicatedDetectionGaugePercent, HearingPercent, bEnableSight ? TEXT("켜짐") : TEXT("꺼짐"), bEnableHearing ? TEXT("켜짐") : TEXT("꺼짐"), DetectionGaugeWidgetComponent->IsVisible() ? TEXT("켜짐") : TEXT("숨김"), DetectionGaugeWidgetComponent->bHiddenInGame ? TEXT("켜짐") : TEXT("꺼짐"), DrawSize.X, DrawSize.Y, *GaugeWidget->GetGaugeBindingDebugInfo(), *RegistrationInfo), FColor::Yellow);
}

//
void AGuardCharacter::SetDrawSightDebugEnabled(bool bInEnabled)
{
	InitializeSightDebugDisplay();
	// 외부 호출은 메시만 제어한다. 생성 시 저장한 디버그 라인 옵션은 변경하지 않는다.
	if (HasAuthority())
	{
		bReplicatedDrawSightDebug = bInEnabled;
		ForceNetUpdate();
	}
	else
	{
		// 스킬을 사용한 클라이언트만 표시할 때는 AIController 없이 로컬 메시를 제어한다.
		bLocalSightMeshVisibilityOverride = true;
		bLocalSightMeshVisible = bInEnabled;
	}
	OnRep_SightDebugState();
}

void AGuardCharacter::InitializeSightDebugDisplay()
{
	if (bSightDebugDisplayInitialized)
	{
		return;
	}
	bSightDebugDisplayInitialized = true;
	bInitialDrawSightDebug = bDrawSightDebug;
	if (HasAuthority())
	{
		bReplicatedDrawSightDebug = bInitialDrawSightDebug;
	}
}

void AGuardCharacter::SetGuardMoveSpeed(float NewMoveSpeed)
{
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->MaxWalkSpeed = NewMoveSpeed;
	}
}

FGenericTeamId AGuardCharacter::GetGenericTeamId() const
{
	const IGenericTeamAgentInterface* ControllerTeamAgent = Cast<IGenericTeamAgentInterface>(GetController());
	return ControllerTeamAgent ? ControllerTeamAgent->GetGenericTeamId() : FGenericTeamId::NoTeam;
}

bool AGuardCharacter::GetPatrolLocation(int32 Index, FVector& OutLocation) const
{
	if (PatrolPoints.Num() == 0)
	{
		return false;


	}

	const int32 SafeIndex = Index % PatrolPoints.Num();
	const AActor* Point = PatrolPoints[SafeIndex];

	if (!IsValid(Point))
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] PatrolPoints[%d] 가 비어 있거나 파괴된 액터를 가리킨다."), *GetName(), SafeIndex);
		return false;
	}

	OutLocation = Point->GetActorLocation();
	return true;
}
