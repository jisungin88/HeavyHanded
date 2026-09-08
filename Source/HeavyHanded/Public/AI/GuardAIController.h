#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/GuardTypes.h"
#include "GameplayTagContainer.h"        // FGameplayTag — 델리게이트 시그니처라 전방 선언 불가
#include "Core/HeistPhase.h"             // EHeistPhaseReason — 같은 이유
#include "GuardAIController.generated.h"


class UBehaviorTree;
class UAIPerceptionComponent;
class UPerceptionMeterComponent;

class AActor;
class AGameStateBase;
class AGuardCharacter;
class AHeistGameState;


class UGuardPatrolAComponent;
class UGuardSightAComponent;
class UGuardHearingAComponent;


// 시야를 놓쳤다가 (혹은 최초로) 플레이어를 다시 포착한 순간에만 발화한다.
// 시야를 유지하는 동안 매 프레임 다시 쏘지 않는다 - false->true 전환 1회.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerSpotted, AActor*, SpottedActor);


UENUM(BlueprintType)
enum class EGuardAIState : uint8
{
	Patrol,
	Search,
	Chase
};


UCLASS()
class AGuardAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGuardAIController();

	// Guard Info (경비 정보)
	// ========================================================
	
	// 경비 개체 종류. 스폰 시 BP_GuardVariant_* 쪽에서 설정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard")
	EGuardType GuardType = EGuardType::Standard;

protected:
	// GuardType 에 맞는 DT_GuardStats 행을 찾아 이동/지각/순찰/조사 수치를 일괄 적용한다.
	// 행을 못 찾으면 위 폴백값을 그대로 두고 경고만 남긴다. InPawn은 이동속도를 적용할 대상.
	void ApplyGuardStats(APawn* InPawn);

	// ========================================================

	UPROPERTY(EditDefaultsOnly, Category = "Guard|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;


	// AI State 상태 관리
	// ========================================================

private:
	UPROPERTY(EditAnywhere) // 테스트용
	//UPROPERTY()
	EGuardAIState AIState = EGuardAIState::Patrol;

public:
	void SetAIState(EGuardAIState NewState) { AIState = NewState; }
	EGuardAIState GetAIState() const { return AIState; }

	bool SelectNextAction(EGuardAIState State);


	// AC (액터컴포넌트)
	// ========================================================

public: // BTT에서 사용하므로
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UGuardPatrolAComponent> GuardPatrolComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UGuardSightAComponent> GuardSightComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UGuardHearingAComponent> GuardHearingComp;





	// Perception (인지)
	// ========================================================

private:
	//VisibleAnywhere 로 두니 bp에서 중복으로 떠 삭제 // lee
	UPROPERTY(BlueprintReadOnly, Category = "Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	// OnPossess 때 빙의한 폰에서 가져와 바인딩해 둔다. HandlePerceptionFull 에서 ResetPerception 에 쓴다
	UPROPERTY()
	TObjectPtr<UPerceptionMeterComponent> PerceptionMeter;


protected:


	// UPerceptionMeterComponent::OnPerceptionFull 콜백. 인지 게이지가 100%에 도달하면
	// 마지막 소음 지점으로 조사를 시작하도록 Blackboard를 갱신하고 게이지를 리셋한다.
	UFUNCTION()
	void HandlePerceptionFull(FVector LastNoiseLocation);


public:

	// AI Perception(Sight+Hearing) 콜백. TargetActor / CanSeeTarget / SoundTargetActor 갱신.
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, struct FAIStimulus Stimulus);

	// 시야로 플레이어를 새로 포착한 순간(false->true 전환)에만 발화. 연출용(효과음 등).
	// 서버 권위 - OnTargetPerceptionUpdated 와 동일하게 HasAuthority() 인 곳에서만 브로드캐스트한다.
	UPROPERTY(BlueprintAssignable, Category = "Guard|Perception")
	FOnPlayerSpotted OnPlayerSpotted;

	// Blackboard의 DetectionGauge(0~100)를 그대로 노출한다. 플레이어 화면의 게이지 위젯이
	// 디버그 모드 없이 이 값을 읽어가도록 하기 위한 UI용 게터.
	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	float GetDetectionGaugePercent() const;

	// 지금 이 경비가 시야로 쫓고 있는 대상이 InActor인지. 게이지 위젯이 "나를 보고 있는
	// 경비"만 골라내는 데 쓴다. CanSeeTarget이 아니라 TargetActor로 판정하는 이유는,
	// 시야를 놓친 직후에도 감소 유예(DecayGraceSeconds) 동안 게이지가 100에 머무르는데
	// 그 구간에도 "이 경비가 나를 쫓고 있다"는 표시는 계속 보여줘야 하기 때문이다.
	UFUNCTION(BlueprintPure, Category = "Guard|Perception")
	bool IsTargeting(const AActor* InActor) const;



	// Perception UI

protected:

	// 로컬 플레이어를 타겟하고 있을 때만 GetDetectionGaugePercent()를 폰의 머리 위
	// 위젯 컴포넌트(UDetectionGaugeWidget)로 밀어넣는다. BTService_UpdateDetectionGauge와
	// 같은 주기(0.1초)로 충분해 매 틱 대신 타이머로 돈다.
	void UpdateHeadGaugeWidget();

	// DT_GuardStats 폴백값. 실제 값은 OnPossess 때 테이블에서 덮어쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Guard|Perception", meta = (ClampMin = "0.01", Units = "s"))
	float HeadGaugeUpdateInterval = 0.1f;

	// ========================================================



public:

	// 다음 순찰 지점을 골라 Blackboard의 PatrolLocation에 써넣는다.
	// Patrol 브랜치 진입 시 BTTask_SelectNextPatrolPoint가 호출한다.
	//
	// 아직 현재 목표에 도착하지 않았다면 지점을 넘기지 않고 그대로 유지한다.
	// 이 함수는 브랜치에 진입할 때마다 불리는데, 시야 획득/상실로 순찰이
	// abort 됐다 재개될 때마다 지점을 건너뛰면 순찰 경로가 망가진다.
	// UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
	// void SelectNextPatrolPoint();


	// 다음 수색 지점을 골라 Blackboard의 InvestigateLocation에 써넣는다.
	// Investigate 브랜치 진입 시 BTTask_SelectSearchPoint가 호출한다.
	//
	// 한 번의 조사는 [마지막 목격 지점] -> [주변 무작위 지점 x SearchSweepCount] 순서로
	// 진행된다. 더 훑을 지점이 없으면 false를 돌려주고, 호출한 태스크가 Failed 로
	// 브랜치를 끝내 순찰로 돌려보낸다.
	//
	// 조사 세션은 SearchStartTime 값으로 구분한다. 그 값이 바뀌면(= 게이지가 다시
	// 가득 찼거나 새 소음을 들었으면) 새 조사로 보고 훑기 횟수를 초기화한다.
	// UFUNCTION(BlueprintCallable, Category = "Guard|Investigate")
	// bool SelectNextSearchPoint();






	// World Alert (월드 경계도)
	// ========================================================

public:
	// 월드 경계도를 0~100 퍼센트로 읽어온다 (GameState에 붙는 UAlertComponent 게이지 기반).
	// BTDecorator_CheckWorldAlert 등이 참조.
	float GetWorldAlertLevel() const;

	// ========================================================



	// Match / Heist Phase (게임 / 하이스트 페이즈)
	// ========================================================

protected:

	// 페이즈가 바뀌었다. 판이 끝나면(Phase.Result) 순찰·조사·추격을 멈춘다.
	//
	// 서버에서만 의미가 있다 — AIController 는 애초에 서버에만 존재한다.
	// 여기서 판정은 하지 않는다. 무엇이 끝인지는 코어 루프가 이미 정했고 여기서는 그 결과만 받는다.
	UFUNCTION()
	void HandleHeistPhaseChanged(FGameplayTag NewPhase, FGameplayTag OldPhase, EHeistPhaseReason Reason);


	// ── 판 종료 감지 ──
	//
	// [왜 GameState 를 구독하는가] 순찰을 멈춰야 할 시점은 AI 가 알 수 있는 사실이 아니다.
	//   경보가 울렸는지, 도주 시간이 끝났는지는 코어 루프가 정한다. AHeistGameState 에는
	//   이미 그 전환을 알리는 OnPhaseChanged 가 있으므로 새 API 를 만들지 않고 그것을 받는다.
	//
	// [왜 BeginPlay 에서 바로 못 붙는가] GameState 가 아직 없을 수 있다. 그때 놓치면
	//   그 경비만 판이 끝나도 계속 순찰한다 — 크래시도 경고도 없다.
	//   AHeistPlayerController 와 UNoiseSubsystem 이 쓰는 것과 같은 패턴이다.

	void BindToGameState(AGameStateBase* GameState);
	void UnbindFromGameState();

	// BT 정지 · 이동 정지 · 지각 정지. 되돌리는 경로는 두지 않는다 —
	// 판이 끝난 뒤 다시 순찰할 일은 없고, 다음 판은 레벨을 새로 연다.
	void StopForMatchEnd();

	FDelegateHandle GameStateSetHandle;

	UPROPERTY(Transient)
	TObjectPtr<AHeistGameState> BoundGameState;


	// ========================================================




	// ========================================================
	// Lifecycle (생명주기)

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;


	// ========================================================




	// Runtime State (실행 상태)
	// ========================================================

private:

	// 이동했음
	/// // 마지막으로 선택된 순찰 지점 인덱스. 다음 호출 시 패턴에 따라 갱신.
	/// int32 CurrentPatrolIndex = -1;
	/// 
	/// // PingPong 패턴에서 현재 진행 방향 (true=정방향/증가, false=역방향/감소)
	/// bool bPatrolMovingForward = true;
	/// 
	/// // 진단용. 순찰 지점 선택 간격을 로그에 남겨 abort/restart 폭주를 구분한다.
	/// // 음수는 "아직 한 번도 고른 적 없음".
	/// float LastPatrolSelectTime = -1.f;
	/// 
	/// 
	/// // 이번 조사에서 지금까지 고른 지점 수. 0 = 마지막 목격 지점 자체.
	/// // -1 은 "이번 조사에서 아직 아무것도 고르지 않음".
	/// int32 CurrentSearchStep = -1;


	
	FTimerHandle HeadGaugeUpdateTimerHandle;


	// ========================================================













};
