#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "AI/GuardTypes.h"
#include "GuardCharacter.generated.h"

class UPerceptionMeterComponent;
class UWidgetComponent;

UCLASS()
class AGuardCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AGuardCharacter();

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

	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	UPerceptionMeterComponent* GetPerceptionMeter() const { return PerceptionMeter; }

	// AGuardAIController가 매 갱신마다 이 컴포넌트의 위젯(UDetectionGaugeWidget)에
	// SetGaugePercent를 직접 호출한다. 위젯 클래스는 BP_GuardBase 등 파생 BP에서
	// WBP_DetectionGauge로 지정한다.
	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	UWidgetComponent* GetDetectionGaugeWidgetComponent() const { return DetectionGaugeWidgetComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception")
	TObjectPtr<UPerceptionMeterComponent> PerceptionMeter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Perception")
	TObjectPtr<UWidgetComponent> DetectionGaugeWidgetComponent;
};
