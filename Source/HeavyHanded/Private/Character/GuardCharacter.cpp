#include "Character/GuardCharacter.h"
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
