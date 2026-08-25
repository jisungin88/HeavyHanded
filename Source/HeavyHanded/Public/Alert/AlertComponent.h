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

/** 병력 증원 발동. 몇 번째 증원인지(1부터)를 같이 준다. 경비 스포너가 구독해 1명씩 추가 스폰한다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReinforcementTriggered, int32, ReinforcementIndex);

/**
 * 저택 전체의 경계도. GameState 에 런타임 부착되며 서버가 계산하고 전원에게 복제된다.
 * 청취자가 아니라 OnNoiseReported(감쇄 전)를 구독한다 — 경계도에는 청취 위치 개념이 없다.
 * 히스테리시스가 있어 단계가 게이지만으로 결정되지 않으므로 둘 다 복제한다.
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
	 * 결과 화면 집계 원본. **서버에만 있다** — 복제하지 않아 클라에서는 항상 비어 있다.
	 * 결과 화면은 아래 GetNoisiestPlayer() 를 쓸 것.
	 */
	const TMap<TObjectPtr<APlayerState>, float>& GetNoiseContribution() const { return NoiseContribution; }

	/**
	 * 결과 화면 "최다 소음 유발자". 서버 · 클라 양쪽에서 유효하다.
	 * 집계 맵 전체가 아니라 1위만 복제한다 — 결과 화면이 필요한 것이 그것뿐이다.
	 * 집계가 비었으면 nullptr 이고 OutContribution 은 0 이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Alert")
	APlayerState* GetNoisiestPlayer(float& OutContribution) const;

	/** 서버 전용. 게이지를 직접 설정한다 (치트 · 스크립트 이벤트용) */
	UFUNCTION(BlueprintCallable, Category = "Alert")
	void SetAlertGauge01(float NewGauge);

	/** 서버 전용. 경보 래치까지 포함해 전부 초기화한다 */
	UFUNCTION(BlueprintCallable, Category = "Alert")
	void ResetAlert();

	/**
	 * 경비 추격이 시작될 때마다 부른다. 서버 전용.
	 * PursuitsPerReinforcement 번마다 증원을 쏘고 카운트를 되돌린다 — 계속 잡히면 계속 증원된다.
	 * ReportNoiseDetected() 와는 별개 카운터라 서로 간섭하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alert")
	void ReportPursuitStarted();

	/**
	 * 경비가 인지 게이지를 가득 채울 때마다 부른다. 서버 전용.
	 * NoiseDetectionsPerReinforcement 번마다 증원을 쏜다. 추격 카운터와 섞이지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Alert")
	void ReportNoiseDetected();

	UPROPERTY(BlueprintAssignable, Category = "Alert")
	FOnAlertLevelChanged OnAlertLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alert")
	FOnAlertGaugeChanged OnAlertGaugeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Alert")
	FOnReinforcementTriggered OnReinforcementTriggered;

protected:
	UFUNCTION()
	void OnRep_ReplicatedGauge();

	UFUNCTION()
	void OnRep_AlertLevel();

	/**
	 * 0~1. 서버 권위. 이 값 자체는 복제하지 않는다 — ReplicatedGauge 가 대신한다.
	 * 서버에서는 정밀한 실수값, 클라에서는 양자화를 되돌린 값이 들어 있다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Alert")
	float AlertGauge = 0.f;

	/**
	 * AlertGauge 를 0~255 로 양자화한 복제본. 해상도 약 0.4%p — HUD 바에는 충분하다.
	 * float 를 그대로 복제하면 30~60초짜리 자연 감소 내내 매 프레임 전송된다.
	 * **단계 판정에는 절대 쓰지 말 것** — 판정은 항상 AlertGauge 원본으로 한다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedGauge)
	uint8 ReplicatedGauge = 0;

	/** 히스테리시스 때문에 게이지에서 유도할 수 없다. 별도로 복제한다 */
	UPROPERTY(ReplicatedUsing = OnRep_AlertLevel, BlueprintReadOnly, Category = "Alert")
	EAlertLevel AlertLevel = EAlertLevel::Calm;

	/**
	 * 현재 1위 소음 유발자. 결과 화면(클라)에서 읽으라고 복제한다.
	 * 기여량이 단조 증가라 갱신이 O(1) 이고, dirty 도 순위가 실제로 뒤집힐 때만 생긴다.
	 */
	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> NoisiestPlayer = nullptr;

	/** NoisiestPlayer 의 누적 기여량 */
	UPROPERTY(Replicated)
	float NoisiestContribution = 0.f;

	// 임계값 · 감소율은 UAlertSettings 에 있다.
	// 이 컴포넌트는 런타임 생성이라 디테일 패널에 안 뜨므로
	// EditAnywhere 를 여기 두면 아무도 만질 수 없다.
	// Project Settings → Game → Alert

private:
	// 권위 판정은 Shared/NetAuthority.h 의 HasServerAuthority(this) 하나로 통일했다

	/** 감쇄 전 소음 1건을 받는다. 서버에서만 호출된다 */
	void HandleNoiseReported(const FNoiseEvent& Event, const FNoiseProfileRow& Profile, AActor* Instigator);

	/** 게이지 변경 + 단계 재평가 + RepNotify. 서버에서만 부를 것 */
	void ApplyGauge(float NewGauge);

	/** 히스테리시스를 적용해 다음 단계를 계산한다 */
	EAlertLevel EvaluateLevel(float Gauge, EAlertLevel Current) const;

	/** 현재 단계에 맞는 무소음 유예 */
	float GetSilenceGrace() const;

	/**
	 * ReportPursuitStarted() / ReportNoiseDetected() 가 공유하는 로직.
	 * Counter 를 올리고 Threshold 에 도달하면 0으로 되돌린 뒤 병력 증원을 발동한다.
	 */
	void ReportTowardReinforcement(int32& Counter, int32 Threshold, const TCHAR* SourceLabel);

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

	/** ReportPursuitStarted() 누적 카운트. PursuitsPerReinforcement 에 도달하면 0으로 되돌린다 */
	int32 PursuitCount = 0;

	/** ReportNoiseDetected() 누적 카운트. NoiseDetectionsPerReinforcement 에 도달하면 0으로 되돌린다 */
	int32 NoiseDetectionCount = 0;

	/** 지금까지 몇 번 증원을 발동했는지. OnReinforcementTriggered 에 몇 번째인지 실어 보낸다 */
	int32 ReinforcementCount = 0;
};
