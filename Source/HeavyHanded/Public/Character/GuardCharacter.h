#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "AI/GuardTypes.h"
#include "GuardCharacter.generated.h"

class UPerceptionMeterComponent;
class UWidgetComponent;

class UProceduralMeshComponent;

UCLASS()
class AGuardCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:

	AGuardCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;
	FTransform GetEyeSocketTransform() const;


	// Guard Info (경비 정보)
	// ========================================================
	// 가드 캐릭터로 이동
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
	EGuardType GuardType = EGuardType::Standard;


private:

	// AllowPrivateAccess
	// 경비의 시야 감지 기능을 활성화할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Perception", meta = (DisplayPriority = 1, AllowPrivateAccess = "true"))
	bool bEnableSight = true;

	// 경비의 청각 감지 기능을 활성화할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Perception", meta = (DisplayPriority = 1, AllowPrivateAccess = "true"))
	bool bEnableHearing = true;

	// 경비의 시야 디버그 표시를 활성화할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Perception", meta = (DisplayPriority = 1, AllowPrivateAccess = "true"))
	bool bDrawSightDebug = true;

	// 켜면 EyeSocket의 위치와 회전을 경비 시야 판정 및 시야 메시 방향에 사용한다.
	// 끄면 캐릭터 위치와 EyeHeight, 기본 화살표 방향을 사용한다. 패키징에서는 캡슐 방향을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Perception", meta = (DisplayPriority = 2, AllowPrivateAccess = "true"))
	bool bUseEyeSocketForSight = true;


	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> SightDebugMesh;

	UPROPERTY(ReplicatedUsing = OnRep_SightDebugState, VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (AllowPrivateAccess = "true"))
	float ReplicatedSightRadius = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SightDebugState, VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (AllowPrivateAccess = "true"))
	float ReplicatedSightHalfAngle = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SightDebugState, VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (AllowPrivateAccess = "true"))
	bool bReplicatedDrawSightDebug = true;

	UPROPERTY(ReplicatedUsing = OnRep_SightDebugRotation, VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (AllowPrivateAccess = "true"))
	FRotator ReplicatedSightDebugRotation = FRotator::ZeroRotator;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception", meta = (AllowPrivateAccess = "true"))
	float ReplicatedDetectionGaugePercent = 0.0f;

	UFUNCTION()
	void OnRep_SightDebugState();

	UFUNCTION()
	void OnRep_SightDebugRotation();


public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Debug", meta = (ClampMin = "0.05", UIMin = "0.05", Units = "s"))
	float SightDebugUpdateInterval = 0.25f;

	// 서버에서 현재 행동의 목표와 별도의 어그로 대상까지 디버그 선을 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard|Debug", meta = (DisplayName = "Draw Move Target Debug"))
	bool bDrawMoveTargetDebug = false;

	bool IsSightEnabled() const { return bEnableSight; }
	bool IsHearingEnabled() const { return bEnableHearing; }
	bool IsDrawSightDebugEnabled() const { return bDrawSightDebug; }
	bool IsEyeSocketSightEnabled() const { return bUseEyeSocketForSight; }
	FRotator GetSightSocketRotation() const;
	float GetSightDebugUpdateInterval() const { return SightDebugUpdateInterval; }

	void SetSightEnabled(bool bInEnabled) { bEnableSight = bInEnabled; }
	void SetHearingEnabled(bool bInEnabled) { bEnableHearing = bInEnabled; }
	UFUNCTION(BlueprintCallable, Category = "Guard|Debug")
	void SetDrawSightDebugEnabled(bool bInEnabled);
	void SetReplicatedSightDebugState(float InSightRadius, float InSightHalfAngle, bool bInEnabled);
	void SetReplicatedSightDebugRotation(FRotator InRotation);
	void SetReplicatedDetectionGauge(float InGaugePercent);
	UFUNCTION(NetMulticast, Unreliable, Category = "Guard|Perception")
	void Multicast_UpdateSightDebugMesh(const TArray<FVector>& FlatVertices, const TArray<int32>& FlatTriangles,
		const TArray<FVector>& GroundVertices, const TArray<int32>& GroundTriangles,
		FRotator InSightRotation, FLinearColor InFanColor, UMaterialInterface* InMaterial);
	float GetReplicatedSightRadius() const { return ReplicatedSightRadius; }
	float GetReplicatedSightHalfAngle() const { return ReplicatedSightHalfAngle; }
	bool GetReplicatedDrawSightDebug() const { return bReplicatedDrawSightDebug; }

	UProceduralMeshComponent* GetSightDebugMesh() const { return SightDebugMesh; }


	//// ---------------------------------------------------------------------------------

	void SetGuardMoveSpeed(float NewMoveSpeed);

	// IGenericTeamAgentInterface 기본 구현(GenericTeamAgentInterface.h)은 감지 대상 액터
	// 자신이 이 인터페이스를 구현했는지만 보고, 그 액터의 컨트롤러까지는 확인하지 않는다.
	// AGuardAIController::SetGenericTeamId 로 컨트롤러에 팀을 심어도 폰(=Sight 가 실제로
	// 검사하는 대상)이 인터페이스를 안 들고 있으면 항상 "중립"으로 판정돼 팀이 무시된다.
	// 그래서 폰에서도 구현해 컨트롤러 값을 그대로 전달한다.
	virtual FGenericTeamId GetGenericTeamId() const override;

	// 레벨에 배치된 경비 인스턴스마다 서로 다른 순찰 경로를 지정할 수 있도록
	// EditInstanceOnly로 노출한다 (블루프린트 기본값이 아니라, 레벨의 각 배치본에서 직접 설정).
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Guard|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	// 순찰 지점을 도는 방식. 기본값은 왕복 - 완전 순환보다 자연스럽고, 완전 무작위보다
	// 플레이어가 패턴을 관찰해 침투 타이밍을 잡을 수 있어 잠입 게임 특성에 더 맞는다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Guard|Patrol")
	EPatrolPattern PatrolPattern = EPatrolPattern::PingPong;

	// 유효 범위를 벗어나면 첫 지점으로 순환한다. 배열이 비어있으면 false를 반환.
	UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	bool GetPatrolLocation(int32 Index, FVector& OutLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	int32 GetPatrolPointCount() const { return PatrolPoints.Num(); }



private:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception", meta = (AllowPrivateAccess = "true"))
	float EyeHeight = 88.0f;

public:

	float GetEyeHeight() const { return EyeHeight; }


	virtual void BeginPlay() override;


	void UpdatePerceptionWidgets();


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception")
	TObjectPtr<UPerceptionMeterComponent> PerceptionMeterComponent;

public:

	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	UPerceptionMeterComponent* GetPerceptionMeterComponent() const
	{ return PerceptionMeterComponent; }



	// 위젯 관리
protected:
	UPROPERTY(EditAnywhere, Category = "Guard|Perception")
	TObjectPtr<UWidgetComponent> DetectionGaugeWidgetComponent;

	UPROPERTY(EditAnywhere, Category = "Guard|Perception")
	TObjectPtr<UWidgetComponent> HearingGaugeWidgetComponent;




public:


	// AGuardAIController가 매 갱신마다 이 컴포넌트의 위젯(UDetectionGaugeWidget)에
	// SetGaugePercent를 직접 호출한다. 위젯 클래스는 BP_GuardBase 등 파생 BP에서
	// WBP_DetectionGauge로 지정한다.
	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	UWidgetComponent* GetDetectionGaugeWidgetComponent() const { return DetectionGaugeWidgetComponent; }

	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	UWidgetComponent* GetHearingGaugeWidgetComponent() const { return HearingGaugeWidgetComponent; }


	void SetHeadGaugeUpdateInterval(float NewInterval);
	void StopHeadGaugeUpdate();
//private:


protected:

	void UpdateHeadGaugeWidget();

	// DT_GuardStats 폴백값. 실제 값은 OnPossess 때 테이블에서 덮어쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.01", Units = "s"))
	float HeadGaugeUpdateInterval = 0.1f;

	FTimerHandle HeadGaugeUpdateTimerHandle;

};
