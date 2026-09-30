#include "Character/GuardCharacter.h"
#include "AI/GuardTypes.h"

//#include "Noise/PerceptionMeterComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Noise/PerceptionMeterComponent.h"

#include "UI/DetectionGaugeWidget.h"
#include "UI/PerceptionMeterWidget.h"

#include "Kismet/GameplayStatics.h"
#include "AI/GuardAIController.h"
#include "AI/GuardBlackboardKeys.h"
#include "BehaviorTree/BlackboardComponent.h"
//#include "AI/GuardSightAComponent.h"

#include "ProceduralMeshComponent.h"
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
	DetectionGaugeWidgetComponent->SetDrawSize(FVector2D(120.f, 16.f));
	DetectionGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	

	// 소리 디버그용 위젯
	HearingGaugeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NoiseGaugeWidgetComponent"));
	HearingGaugeWidgetComponent->SetupAttachment(GetCapsuleComponent());
	HearingGaugeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // 나중에 다시 고칠 것
	HearingGaugeWidgetComponent->SetDrawSize(FVector2D(120.f, 16.f));
	HearingGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 135.f));
	// HearingGaugeWidgetComponent->SetDrawAtDesiredSize(false);


	SightDebugMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SightDebugMesh"));
	SightDebugMesh->SetupAttachment(GetRootComponent());
	SightDebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SightDebugMesh->SetCastShadow(false);

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
	// EyeSocket의 로컬 정면 축이 경비 시야 정면에서 왼쪽으로 90도 돌아 있어
	// 소켓 회전을 그대로 쓰면 메시와 AI Perception이 함께 옆을 향한다.
	FRotator SightRotation = GetEyeSocketTransform().Rotator();
	SightRotation.Yaw += 90.0f;
	return SightRotation;
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
}

void AGuardCharacter::OnRep_SightDebugState()
{
	if (IsValid(SightDebugMesh))
	{
		SightDebugMesh->SetVisibility(bReplicatedDrawSightDebug);
	}

}

void AGuardCharacter::OnRep_SightDebugRotation()
{
	if (IsValid(SightDebugMesh))
	{
		SightDebugMesh->SetWorldRotation(ReplicatedSightDebugRotation);
	}
}

void AGuardCharacter::SetReplicatedSightDebugState(float InSightRadius, float InSightHalfAngle, bool bInEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedSightRadius = InSightRadius;
	ReplicatedSightHalfAngle = InSightHalfAngle;
	bReplicatedDrawSightDebug = bInEnabled;
	if (IsValid(SightDebugMesh))
	{
		SightDebugMesh->SetVisibility(bReplicatedDrawSightDebug);
	}
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

void AGuardCharacter::Multicast_UpdateSightDebugMesh_Implementation(
	const TArray<FVector>& FlatVertices, const TArray<int32>& FlatTriangles,
	const TArray<FVector>& GroundVertices, const TArray<int32>& GroundTriangles,
	FRotator InSightRotation, FLinearColor InFanColor, UMaterialInterface* InMaterial)
{
	if (!IsValid(SightDebugMesh) || !bReplicatedDrawSightDebug || FlatVertices.IsEmpty() || FlatTriangles.IsEmpty())
	{
		return;
	}

	SightDebugMesh->SetWorldRotation(InSightRotation);
	TArray<FVector> FlatNormals;
	TArray<FVector2D> FlatUV0;
	TArray<FLinearColor> FlatVertexColors;
	FlatNormals.Init(FVector::UpVector, FlatVertices.Num());
	FlatUV0.Init(FVector2D::ZeroVector, FlatVertices.Num());
	FlatVertexColors.Init(InFanColor, FlatVertices.Num());
	TArray<FProcMeshTangent> FlatTangents;

	SightDebugMesh->ClearAllMeshSections();
	SightDebugMesh->CreateMeshSection_LinearColor(
		0, FlatVertices, FlatTriangles, FlatNormals, FlatUV0, FlatVertexColors, FlatTangents, false);

	if (GroundVertices.Num() > 0 && GroundTriangles.Num() > 0)
	{
		TArray<FVector> GroundNormals;
		TArray<FVector2D> GroundUV0;
		TArray<FLinearColor> GroundVertexColors;
		GroundNormals.Init(FVector::UpVector, GroundVertices.Num());
		GroundUV0.Init(FVector2D::ZeroVector, GroundVertices.Num());
		GroundVertexColors.Init(InFanColor, GroundVertices.Num());
		TArray<FProcMeshTangent> GroundTangents;
		SightDebugMesh->CreateMeshSection_LinearColor(
			1, GroundVertices, GroundTriangles, GroundNormals, GroundUV0, GroundVertexColors, GroundTangents, false);
	}

	if (IsValid(InMaterial))
	{
		SightDebugMesh->SetMaterial(0, InMaterial);
		if (GroundVertices.Num() > 0 && GroundTriangles.Num() > 0)
		{
			SightDebugMesh->SetMaterial(1, InMaterial);
		}
	}

	SightDebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SightDebugMesh->SetVisibility(true);
	SightDebugMesh->SetHiddenInGame(false);
}


void AGuardCharacter::BeginPlay()
{
	Super::BeginPlay();
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

	//UE_LOG(LogGuardAI, Warning, TEXT("[%s] PerceptionMeterComponent=%s"), *GetNameSafe(this), *GetNameSafe(PerceptionMeterComponent));

	UPerceptionMeterWidget* PerceptionWidget = Cast<UPerceptionMeterWidget>(HearingGaugeWidgetComponent->GetUserWidgetObject());
	if (!PerceptionWidget)
	{
		UE_LOG(LogGuardAI, Warning,
			TEXT("[%s] HearingGaugeWidgetComponent에서 PerceptionMeterWidget을 가져오지 못했습니다."), *GetName());
		return;
	}

	//UE_LOG(LogGuardAI, Warning, TEXT("[%s] PerceptionMeterWidget 연결 성공."), *GetName());
	PerceptionWidget->BindToGuard(this);



	if (HearingGaugeWidgetComponent)
	{
		UUserWidget* Widget = HearingGaugeWidgetComponent->GetUserWidgetObject();

		//UE_LOG(LogGuardAI, Warning, TEXT("[%s] Hearing DrawSize = %s"), *GetName(), *HearingGaugeWidgetComponent->GetDrawSize().ToString());

		if (Widget)
		{
			//UE_LOG(LogGuardAI, Warning, TEXT("[%s] Hearing DesiredSize = %s"), *GetName(), *Widget->GetDesiredSize().ToString());
		}
	}

	UpdatePerceptionWidgets();


}

void AGuardCharacter::UpdatePerceptionWidgets()
{

	if (DetectionGaugeWidgetComponent)
	{
		DetectionGaugeWidgetComponent->SetVisibility(bEnableSight);
		bEnableSight ? DetectionGaugeWidgetComponent->Activate() : DetectionGaugeWidgetComponent->Deactivate();
	}

	if (HearingGaugeWidgetComponent)
	{
		HearingGaugeWidgetComponent->SetVisibility(bEnableHearing);
		bEnableHearing ? HearingGaugeWidgetComponent->Activate() : HearingGaugeWidgetComponent->Deactivate();
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
	if (HasAuthority())
	{
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
		}
	}

	if (!IsValid(DetectionGaugeWidgetComponent))
	{
		return;
	}

	UDetectionGaugeWidget* GaugeWidget = Cast<UDetectionGaugeWidget>(DetectionGaugeWidgetComponent->GetUserWidgetObject());
	if (!GaugeWidget)
	{
		// Widget Class 가 아직 지정 안 됐거나(파생 BP에서 WBP_DetectionGauge 미설정),
		// 컴포넌트가 아직 위젯 인스턴스를 만들기 전(BeginPlay 타이밍)일 수 있다.
		return;
	}

	GaugeWidget->SetGaugePercent(ReplicatedDetectionGaugePercent);
}

//
void AGuardCharacter::SetDrawSightDebugEnabled(bool bInEnabled)
{
	bDrawSightDebug = bInEnabled;

	//if (AGuardAIController* GuardController = Cast<AGuardAIController>(GetController()))
	//{
	//	if (GuardController->GuardSightComp)
	//	{
	//		GuardController->GuardSightComp->SetSightDebugEnabled(bInEnabled);
	//	}
	//}

	AGuardAIController* GuardController = Cast<AGuardAIController>(GetController());
	if (!GuardController)
	{
		return;
	}

	GuardController->SetSightDebugEnabled(bInEnabled);

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
