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

	//UFUNCTION()
	//void HandlePerceptionChanged(float NewPerception01);

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


};
