#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AITypes.h"                  // FAIStimulus — UFUNCTION 값 인자
#include "AI/Navigation/NavigationTypes.h"
#include "AI/GuardTypes.h"
#include "GameplayTagContainer.h"        // FGameplayTag — 델리게이트 시그니처라 전방 선언 불가
#include "Core/HeistPhase.h"             // EHeistPhaseReason — 같은 이유
#include "Noise/NoiseTypes.h"
#include "GuardAIController.generated.h"


class UBehaviorTree;
class UAIPerceptionComponent;

class AActor;
class AGameStateBase;
class AGuardCharacter;
class ABaseCharacter;
class AHeistGameState;


class UGuardPatrolAComponent;
class UGuardSightAComponent;
class UGuardHearingAComponent;
class UAlertComponent;


// 시야를 놓쳤다가 (혹은 최초로) 플레이어를 다시 포착한 순간에만 발화한다.
// 시야를 유지하는 동안 매 프레임 다시 쏘지 않는다 - false->true 전환 1회.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerSpotted, AActor*, SpottedActor);


UENUM(BlueprintType)
enum class EGuardAIState : uint8 // GuardTypes.h로 옮기는 작업 필요
{
	Patrol,
	Search,
	Chase,
	// [체포 추가] 기존 상태의 숫자를 유지하도록 마지막에 추가한다. 체포한 플레이어를 감시하는 상태다.
	Custody
};


UCLASS()
class AGuardAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGuardAIController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;
	virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const override;




	// Guard Info (경비 정보)
	// ========================================================

protected:

	UPROPERTY()
	TObjectPtr<AGuardCharacter> PossessGuardPawn; // 빙의할 가드 Pawn

	// GuardType 에 맞는 DT_GuardStats 행을 찾아 이동/지각/순찰/조사 수치를 일괄 적용한다.
	// 행을 못 찾으면 위 폴백값을 그대로 두고 경고만 남긴다. InPawn은 이동속도를 적용할 대상.
	void ApplyGuardStats(); //APawn* InPawn);

	// ========================================================

	UPROPERTY(EditDefaultsOnly, Category = "Guard|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

public:

	AGuardCharacter* GetPossessGuardPawn() const { return PossessGuardPawn; }
	FString GetPossessGuardPawnName() const;

	UGuardSightAComponent* GetGuardSightComponent() const { return GuardSightComp; }
	void SetSightDebugEnabled(bool bInEnabled);

	// 기본은 한글 요약 로그이며, 활성화하면 주기 상태와 전체 진단 정보를 출력한다.
	UPROPERTY(EditAnywhere, Category = "Guard|Debug", meta = (DisplayName = "시야·수색 상세 로그"))
	bool bDetailedSightSearchLogs = false;



	// AI State 상태 관리
	// ========================================================

private:
	UPROPERTY(EditAnywhere, Category = "Guard|AI") // 테스트용
	//UPROPERTY()
	EGuardAIState AIState = EGuardAIState::Patrol;

	// const 경로 계산에서도 진단 출력 간격을 기록한다. AI/Blackboard 상태는 변경하지 않는다.
	mutable TMap<FName, float> SearchDebugLastLogTimes;
	FString LastChaseMoveRequestDebug;
	mutable FString LastChasePathDebug;
	mutable float LastArrestRangeDebug = -1.f;
	bool IsVisibleChaseMove(const FAIMoveRequest& MoveRequest) const;
	void HandleChasePathUpdated(FNavigationPath* UpdatedPath, ENavPathEvent::Type Event) const;

public:
	void SetAIState(EGuardAIState NewState);
	EGuardAIState GetAIState() const { return AIState; }

	// [체포 추가] 이 상태에서는 새 타겟·소음·경보 수색으로 감시를 중단하지 않는다.
	UFUNCTION(BlueprintPure, Category = "Guard|Custody")
	bool IsInCustody() const { return AIState == EGuardAIState::Custody; }
	// [체포 UI] 일반 TargetActor를 비운 뒤에도 감시 대상 Portrait를 유지하기 위한 서버 조회다.
	ABaseCharacter* GetCustodyTarget() const { return CustodyTarget; }

	// [체포 추가] 체포 태스크가 성공 처리된 뒤 호출한다. BT를 정지하고 체포 대상 감시를 맡는다.
	void BeginCustody(ABaseCharacter* Target);
	// [체포 추가] 구출·플레이어 제거·경비 빙의 해제 시 감시를 종료한다.
	// ReleasedTarget을 전달하면 현재 감시 대상과 일치할 때만 종료하여 잘못된 해제 요청을 막는다.
	void EndCustody(ABaseCharacter* ReleasedTarget = nullptr);

private:
	// [체포 추가] 일반 추격 Blackboard의 TargetActor와 분리한다.
	// 다른 경비의 체포 대상 제외 처리가 이 경비의 감시 대상까지 지우지 않도록 별도로 보관한다.
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Guard|Custody")
	TObjectPtr<ABaseCharacter> CustodyTarget;

	// [체포 추가] 대상이 외부 힘·운반으로 이 거리보다 멀어지면 다시 접근한다.
	// 체포 성공 거리(ArrestRange)와 별개인 감시 중 수평 거리이며, 기본값은 180cm다.
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Custody", meta = (ClampMin = "50.0", Units = "cm"))
	float CustodyFollowDistance = 180.f;

	// [체포 추가] 감시 대상에게 재접근하는 경로 요청을 초당 한 번으로 제한한다.
	float NextCustodyMoveTime = 0.f;

public:

	bool SelectNextAction(EGuardAIState State);

	// 수색 전환 진단용. 동일 이벤트는 경비별 초당 한 번만 출력한다.
	void LogSearchTransitionDebug(FName Event, const FString& Detail) const;
	void LogArrestRangeDebug(float ArrestRange, bool bMoveCompleted = false) const;


	// AC (액터컴포넌트)
	// ========================================================

public: // BTT에서 사용하므로
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Components")
	TObjectPtr<class UGuardPatrolAComponent> GuardPatrolComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Components")
	TObjectPtr<class UGuardSightAComponent> GuardSightComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Components")
	TObjectPtr<class UGuardHearingAComponent> GuardHearingComp;





	// Perception (인지)
	// ========================================================

private:
	//VisibleAnywhere 로 두니 bp에서 중복으로 떠 삭제 // lee
	UPROPERTY(BlueprintReadOnly, Category = "Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;


protected:


	// UPerceptionMeterComponent::OnPerceptionFull 콜백. 인지 게이지가 100%에 도달하면
	// RequestInvestigate() 로 조사를 시작시키고, 이 경로에서만 의미 있는 게이지 리셋을 마저 한다.
	UFUNCTION()
	void HandlePerceptionFull(FVector LastNoiseLocation);

public:

	// 소음(청각 게이지)이 아닌 다른 경로(카메라 등 Hazard)에서도 "여기를 조사하라" 고
	// 지시할 수 있는 공개 진입점. HandlePerceptionFull 의 Blackboard 갱신 부분과 동일한
	// 처리를 공유한다 — 조사 시작이라는 결과는 감지 수단이 무엇이든 같아야 하기 때문이다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Perception")
	void RequestInvestigate(FVector Location);



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
	// ==================================================================================

	// Controller는 "경계도 때문에 속도를 올릴지"만 판단하고
	// Hearing은 "소음이 계속 발생하고 있는지 / 20초 동안 조용했는지"를 관리하는 구조
private:
	// 현재 추격 및 월드 경계도 상태를 기준으로 최종 이동 속도를 적용한다.
	void ApplyCurrentMoveSpeed();
	void BindToWorldAlert();
	void EnsureAlarmSearchSession();

	UFUNCTION(Category = "Guard|Alert")
	void HandleWorldAlertLevelChanged(EAlertLevel NewLevel, EAlertLevel OldLevel);

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Guard|Alert")
	TObjectPtr<UAlertComponent> BoundAlertComponent;

	bool bWorldAlarmBehaviorActive = false;
	bool bMatchEnded = false;

public:
	// 월드 경계도를 0~100 퍼센트로 읽어온다 (GameState에 붙는 UAlertComponent 게이지 기반)
	// BTDecorator_CheckWorldAlert 등이 참조.
	float GetWorldAlertLevel() const;					// 1
	bool IsWorldAlarmActive() const;
	// 기존 게이지 서비스에서도 확인해 GameState 초기화 순서가 늦어져도 경보를 놓치지 않는다.
	void UpdateWorldAlarmBehavior();



	// 현재 월드 경계도에 따라 경비의 이동 속도를 갱신한다.
	// 임계값을 넘으면 경계 속도를 적용하고, 다시 내려가면 기본 속도로 복구한다.
	UFUNCTION() // AddDynamic을 쓰려면 UpdateMoveSpeedByWorldAlert()에 UFUNCTION()이 필요
	void UpdateMoveSpeedByWorldAlert(float NewGauge01);		// 2


	// Hearing이 타이머를 가지고 있으려면 Controller가 지금 속도 증가 상태인지를 알아야 함
	// 현재 월드 경계도로 인해 이동 속도가 증가된 상태인지 반환한다.

	bool IsWorldAlertSpeedUp() const { return WorldAlertSet.bWorldAlertSpeedUp; }   // 3
	void SetWorldAlertSpeedUp(bool bInSpeedUp) { WorldAlertSet.bWorldAlertSpeedUp = bInSpeedUp; }
	
	float GetNormalMoveSpeed() const { return WorldAlertSet.NormalMoveSpeed; }    // 4
	void SetNormalMoveSpeed(float NormalSpeed) { WorldAlertSet.NormalMoveSpeed = NormalSpeed; }  // 5

	float GetChaseMoveSpeed() const { return WorldAlertSet.ChaseMoveSpeed; }
	void SetChaseMoveSpeed(float ChaseSpeed) { WorldAlertSet.ChaseMoveSpeed = ChaseSpeed; }

	bool IsChasing() const { return WorldAlertSet.bIsChasing; }
	void SetChasing(bool bChasing);


	float GetWorldAlertSilenceDelay() const { return WorldAlertSet.WorldAlertSilenceDelay; }



private:

	// 월드 경계도에 따른 경비 이동 속도 설정 및 런타임 상태.
	// 실제 월드 경계도 게이지는 UAlertComponent가 관리하고,
	// 이 구조체는 경비가 해당 경계도에 반응하는 데 필요한 값만 관리한다.
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Movement")
	FGuardWorldAlertSettings WorldAlertSet;




public:
	// 월드 경계도에 의해 증가된 이동 속도를 기본 속도로 복구한다.
	void ResetMoveSpeed();



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
	virtual void OnUnPossess() override;


	// ========================================================


};
