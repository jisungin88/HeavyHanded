#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Noise/NoiseTypes.h"          // EAlertLevel — UPROPERTY 노출 enum 이라 전방 선언 불가
#include "AlertComponent.generated.h"

class AGameStateBase;
class APlayerState;
struct FNoiseEvent;
struct FNoiseProfileRow;

/** 경계 단계 전환. 셔터 폐쇄 · 경비 증원이 여기에 붙는다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAlertLevelChanged,
	  EAlertLevel, NewLevel, EAlertLevel, OldLevel);

/** 게이지 값 변화. HUD 위젯용 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAlertGaugeChanged, float, NewGauge01);

/**
 * 저택 전체의 경계도. GameState 에 런타임 부착되며 서버가 계산하고 전원에게 복제된다.
 *
 * 소음 서브시스템의 OnNoiseReported(감쇄 전)를 구독한다. 청취자가 아닌 이유는
 * 경계도에 청취 위치 개념이 없기 때문이다 — 벽 너머에서 금고를 뜯어도 경계도는 똑같이 오른다.
 *
 * 히스테리시스가 있어서 단계는 게이지만으로 결정되지 않는다. 그래서 둘 다 복제한다.
 */
UCLASS(ClassGroup = (Alert))
class HEAVYHANDED_API UAlertComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAlertComponent();

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
													   FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End

	/** 어디서든 이걸로 접근. GameState 에 붙어 있는 인스턴스를 찾는다 */
	UFUNCTION(BlueprintPure, Category = "Alert", meta = (WorldContext = "WorldContext"))
	static UAlertComponent* Get(const UObject* WorldContext);

	/** 서버 전용. GameState 에 부착한다. 이미 있으면 그것을 돌려준다 */
	static UAlertComponent* EnsureOnGameState(AGameStateBase* GameState);

	UFUNCTION(BlueprintPure, Category = "Alert")
	float GetAlertGauge01() const { return AlertGauge; }

	UFUNCTION(BlueprintPure, Category = "Alert")
	EAlertLevel GetAlertLevel() const { return AlertLevel; }

	/** 경보(래치) 상태인가 */
	UFUNCTION(BlueprintPure, Category = "Alert")
	bool IsAlarmed() const { return AlertLevel == EAlertLevel::Alarm; }

	/**
	 * 결과 화면 집계 원본. 값은 누적 경계도 기여량.
	 * UFUNCTION 파라미터·반환 타입에는 TObjectPtr 를 쓸 수 없어서 C++ 전용이다
	 */
	const TMap<TObjectPtr<APlayerState>, float>& GetNoiseContribution() const { return NoiseContribution; }

	/**
	 * 결과 화면 "최다 소음 유발자" (기획서 8장).
	 * @param OutContribution  그 플레이어의 누적 기여량. 아무도 없으면 0
	 * @return                 최다 유발자. 집계가 비었으면 nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Alert")
	APlayerState* GetNoisiestPlayer(float& OutContribution) const;

	/** 서버 전용. 게이지를 직접 설정한다 (치트 · 스크립트 이벤트용) */
	UFUNCTION(BlueprintCallable, Category = "Alert")
	void SetAlertGauge01(float NewGauge);

	/** 서버 전용. 경보 래치까지 포함해 전부 초기화한다 */
	UFUNCTION(BlueprintCallable, Category = "Alert")
	void ResetAlert();

	UPROPERTY(BlueprintAssignable, Category = "Alert")
	FOnAlertLevelChanged OnAlertLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alert")
	FOnAlertGaugeChanged OnAlertGaugeChanged;

protected:
	UFUNCTION()
	void OnRep_AlertGauge();

	UFUNCTION()
	void OnRep_AlertLevel();

	/** 0~1. 서버 권위 */
	UPROPERTY(ReplicatedUsing = OnRep_AlertGauge, BlueprintReadOnly, Category = "Alert")
	float AlertGauge = 0.f;

	/** 히스테리시스 때문에 게이지에서 유도할 수 없다. 별도로 복제한다 */
	UPROPERTY(ReplicatedUsing = OnRep_AlertLevel, BlueprintReadOnly, Category = "Alert")
	EAlertLevel AlertLevel = EAlertLevel::Calm;

	// 임계값 · 감소율은 UAlertSettings 에 있다.
	// 이 컴포넌트는 런타임 생성이라 디테일 패널에 안 뜨므로
	// EditAnywhere 를 여기 두면 아무도 만질 수 없다.
	// Project Settings → Game → Alert

private:
	bool HasAlertAuthority() const;

	/** 감쇄 전 소음 1건을 받는다. 서버에서만 호출된다 */
	void HandleNoiseReported(const FNoiseEvent& Event, const FNoiseProfileRow& Profile, AActor* Instigator);

	/** 게이지 변경 + 단계 재평가 + RepNotify. 서버에서만 부를 것 */
	void ApplyGauge(float NewGauge);

	/** 히스테리시스를 적용해 다음 단계를 계산한다 */
	EAlertLevel EvaluateLevel(float Gauge, EAlertLevel Current) const;

	/** 현재 단계에 맞는 무소음 유예 */
	float GetSilenceGrace() const;

	/** 결과 화면 집계. PlayerState 는 매치 내내 살아 있으므로 하드 참조로 잡는다 */
	UPROPERTY()
	TMap<TObjectPtr<APlayerState>, float> NoiseContribution;

	float SilenceTimer = 0.f;

	/**
	 * OnAlertLevelChanged 로 마지막에 알린 단계.
	 * 복제는 "바뀌었다"만 전달하고 이전 값은 안 주므로, 구독자에게 정확한 OldLevel 을
	 * 주려면 이쪽에서 들고 있어야 한다. 복제하지 않는다 — 각자 자기가 알린 값을 기억한다
	 */
	EAlertLevel LastBroadcastLevel = EAlertLevel::Calm;

	FDelegateHandle NoiseReportedHandle;
};
