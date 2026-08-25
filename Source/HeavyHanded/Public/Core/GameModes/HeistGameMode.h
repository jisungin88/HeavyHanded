#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameplayTagContainer.h"        // FGameplayTag 를 값으로 주고받는다
#include "Core/HeistPhase.h"             // EHeistPhaseReason — 인자 타입
#include "Noise/NoiseTypes.h"            // EAlertLevel — 다이내믹 델리게이트 시그니처라 전방 선언 불가
#include "HeistGameMode.generated.h"

struct FHeistStartConditions;
class AHeistEntryPoint;
class AVanZone;

/**
 * 작업 레벨의 상태머신 구동부. Prep → Heist → Escape → Result.
 * 전이 규칙은 HeistPhase, 접속 대기 판정은 HeistStartGate, 상태와 복제는 AHeistGameState 가 갖는다.
 * 여기 남는 것은 타이머 · 로그 · 다른 시스템 호출 같은 부수효과뿐이다.
 */
UCLASS()
class HEAVYHANDED_API AHeistGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AHeistGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/**
	 * 접속한 사람의 PlayerState 가 만들어진 직후. **체포자를 관전자로 표시하는 자리다.**
	 * PostLogin 은 Super 안에서 이미 폰을 스폰해 버려서 늦다. 여기는 Login 단계라 그 전이고,
	 * 체포 명단의 키인 FUniqueNetIdRepl 을 인자로 받는다.
	 */
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void Logout(AController* Exiting) override;

	/**
	 * 스폰 위치를 고른다. **전원이 같은 진입점에서 시작한다** — 밴에서 같이 내리는 것이 전제다.
	 * 진입점이 하나도 없는 레벨에서는 엔진 기본 동작으로 돌아간다.
	 */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/**
	 * 페이즈를 즉시 다음으로 넘긴다. 접속 대기 중이면 기다리지 않고 준비 시간을 시작한다.
	 * 치트(hh.Phase.Next)의 진입점이라 사유가 Cheat 로 기록된다.
	 */
	void AdvancePhase(EHeistPhaseReason Reason);

protected:
	virtual void HandleMatchHasStarted() override;

	/**
	 * 페이즈에 들어간 직후, 다른 시스템에 걸어야 하는 것들.
	 * 진입 부수효과는 앞으로 늘어난다 — EnterPhase 에 조건문으로 쌓지 말고 여기 모을 것.
	 */
	virtual void OnPhaseEntered(const FGameplayTag& Phase, EHeistPhaseReason Reason);

	/**
	 * 경계 단계가 바뀌었다. 경보(래치)면 본 작업을 즉시 끝내고 도주로 넘긴다.
	 * 준비 시간의 경보는 무시한다 — Heist 진입에서 어차피 지워지므로 작업을 시작도 못 하고 끝난다.
	 */
	UFUNCTION()
	void HandleAlertLevelChanged(EAlertLevel NewLevel, EAlertLevel OldLevel);

	/** 승차 명단이 바뀌었다. 생존자가 다 탔으면 판을 끝낸다 */
	UFUNCTION()
	void HandleBoardedChanged(int32 NumBoarded, int32 NumSurvivors);

	/** 결과 확인 인원이 바뀌었다. 전원이 확인했으면 체류 시간을 기다리지 않는다 */
	UFUNCTION()
	void HandleResultConfirmChanged(int32 NumConfirmed, int32 TotalNum);

	/**
	 * 매치를 끝낸다. 결과 화면에서 나가는 유일한 출구다. (서버 전용, 한 번만)
	 * 기본 구현은 로그만 남긴다 — ServerTravel 은 세션 파트 소관이라 BP 에서 재정의해 붙인다.
	 * 전원 확인과 결과 체류 시간 중 먼저 오는 쪽에 불린다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Heist")
	void FinishMatch();
	virtual void FinishMatch_Implementation();

	/**
	 * 밴을 진입점으로 옮긴다. (서버 전용, 레벨당 한 번, 준비 시간보다 먼저)
	 *
	 * 기본 구현은 순간이동이다. 달려 들어오는 연출은 BP 에서 이 함수만 재정의하면 된다 —
	 * 단 **끝나는 자리가 EntryTransform 이어야 한다.** 플레이어가 이미 그 기준으로 스폰돼 있다.
	 * 서버에서만 돌고 클라이언트에는 이동 복제로 결과만 간다 (연출은 보간되어 보인다).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Heist|Entry")
	void PlaceVan(AVanZone* Van, const FTransform& EntryTransform);
	virtual void PlaceVan_Implementation(AVanZone* Van, const FTransform& EntryTransform);

	/** 이 장소의 목표 금액($). 장소마다 다르므로 UHeistSettings 가 아니라 여기 있다 */
	UPROPERTY(EditDefaultsOnly, Category = "Heist", meta = (ClampMin = "0"))
	int32 TargetValue = 50000;

	/** 본 작업(Phase.Heist) 제한 시간. 준비 시간은 포함되지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Heist", meta = (ClampMin = "1.0", Units = "s"))
	float HeistSeconds = 420.f;

	/**
	 * 이 작업 레벨이 어느 장소인가 (Site.*). 캠페인 진행을 기록하는 키다.
	 * 기본값을 주지 않는다 — 저택 값을 박아 두면 박물관 BP 가 지정을 잊었을 때
	 * 조용히 저택을 두 번 통과한 것으로 기록된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Heist")
	FGameplayTag SiteTag;

private:
	/** 이 판에 몇 명이 올 예정인지 알아낸다. 모르면 0. 우선순위는 구현부 참고 */
	int32 ResolveExpectedPlayers(const FString& Options) const;

	/**
	 * 지금 상황을 판정용 값으로 옮긴다.
	 * (const 가 아닌 것은 AGameMode::GetNumPlayers() 가 non-const 이기 때문이다)
	 */
	FHeistStartConditions MakeStartConditions();

	/**
	 * 이 판의 진입점을 정하고 캐시한다. 레벨당 한 번, 첫 스폰보다 먼저.
	 * ChoosePlayerStart 는 사람마다 불리므로 매번 판정하면 **사람마다 다른 곳에서 시작한다.**
	 */
	void ResolveEntryPoint();

	/**
	 * 밴을 진입점으로 보낸다. 진입점이나 밴이 없으면 아무것도 하지 않는다.
	 * **밴 배치가 실패해도 스폰은 살아 있어야 한다** — 예전에 밴을 스폰 지점에 세워
	 * 폰이 콜리전에 막혀 아무도 움직이지 못했다.
	 */
	void MoveVanToEntry();

	/**
	 * 밴이 스폰 지점을 덮고 있으면 경고한다.
	 * 그때 증상은 "이동도 회전도 안 된다" 뿐이라 원인을 짐작할 수 없다.
	 */
	void WarnIfVanBlocksEntry() const;

	/** 주기적으로 HeistStartGate 에 물어보고, 답에 따라 시작하거나 계속 기다린다 */
	void TickStartWait();

	/** 접속 대기를 끝내고 Phase.Prep 으로 들어간다 */
	void StartPrep();

	/**
	 * 접속 대기 상태를 GameState 로 옮긴다. 클라이언트의 로딩 표시와 입력 차단이 여기 매달린다.
	 * 서버가 할 수 있는 것은 사실을 복제하는 것까지고, 무엇을 할지는 PlayerController 가 정한다.
	 */
	void PublishStartWait(const FHeistStartConditions& Conditions, bool bWaiting);

	/** 페이즈를 바꾸고 다음 전환을 예약한다 */
	void EnterPhase(const FGameplayTag& Phase, EHeistPhaseReason Reason);

	/** 페이즈 타이머 만료 — 시간 만료 경로의 전이 */
	void HandlePhaseElapsed();

	/** 페이즈별 지속 시간(초). 0 이면 카운트다운 없이 머문다 */
	float GetPhaseDuration(const FGameplayTag& Phase) const;

	/**
	 * 탈출 판정을 다음 틱에 예약한다.
	 *
	 * 그 자리에서 판정하면 명단을 바꾸던 코드가 끝나기 전에 결과 화면이 열린다 —
	 * 마지막에 탄 사람의 이벤트가 Result 진입 뒤에 발송된 적이 있다. 예약은 한 틱에 하나다.
	 */
	void RequestEscapeCheck();

	/**
	 * 생존자가 전부 밴에 탔으면 결과로 넘긴다. 직접 부르지 말고 RequestEscapeCheck() 를 쓸 것.
	 * 계기가 셋이라(승차 · 다운 · 접속 종료) 판정은 한 곳이어야 한다.
	 * 준비 시간은 제외한다 — 스폰 직후 전원이 볼륨 안이라 $0 으로 판이 끝난다.
	 */
	void TryFinishByEscape();

	/** 결과 진입 시 체포를 확정한다. 미승차자와 다운자가 대상이다 */
	void ResolveArrests();

	/** 적재 금액을 팀 공용 골드로 넘긴다. 기준은 UHeistSettings::MinOutcomeForPayout */
	void PayoutTeamGold();

	/**
	 * 이 장소를 통과했으면 런 진행에 기록한다. 통과 기준은 등급 `Success` 뿐이다.
	 * 통과 뒤에 어디로 가는가는 여기서 정하지 않는다 — 사실만 남기고 이동은 FinishMatch 가 정한다.
	 */
	void RecordSiteProgress();

	/**
	 * 체포된 사람을 런 진행으로 넘긴다. 등급이나 지급 여부와 무관하게 항상 넘긴다.
	 * 안 넘기면 레벨이 바뀔 때 GameState 도 PlayerState 도 새로 생겨 잡힌 사실이 사라진다.
	 */
	void CarryOverArrests();

	/**
	 * 이 판을 관전으로 보낸 사람들의 체포를 푼다. 한 판 관전이 곧 형기다.
	 * 관전자는 IsCountedPlayer 에서 걸러지므로 CarryOverArrests 와 겹치지 않는다.
	 */
	void ReleaseServedSpectators();

	/**
	 * 이 플레이어의 다운 상태 변화를 구독한다. 다운도 종료 계기다 —
	 * 마지막 한 명이 밖에서 쓰러지면 그 순간 생존자가 이미 밴에 다 타 있는 상태가 된다.
	 * 다운 시스템(전영배)이 아직 태그를 붙이지 않아 지금은 불리지 않는다.
	 */
	void WatchDownedState(APlayerState* Player);

	/** 다운 태그가 붙거나 떨어졌다 */
	void HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount);

	/**
	 * 이 판의 진입점. 한 번 정해지면 판이 끝날 때까지 바뀌지 않는다.
	 * nullptr 이면 진입점이 없는 레벨이다 (아직 안 정한 상태와의 구분은 bEntryResolved).
	 */
	UPROPERTY()
	TObjectPtr<AHeistEntryPoint> ResolvedEntry = nullptr;

	/** 진입점 판정을 이미 했는가. 진입점이 없는 레벨에서 매 스폰마다 다시 훑지 않게 한다 */
	bool bEntryResolved = false;

	/** 밴을 이미 보냈는가. 재정의된 PlaceVan 이 연출이면 두 번 불릴 때 밴이 두 번 달려 들어온다 */
	bool bVanPlaced = false;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle StartWaitHandle;

	/** 접속 대기 상한 시각(월드 시간). 인원이 덜 차도 이때는 시작한다 */
	float StartDeadline = 0.f;

	/**
	 * 이 판에 올 인원. 0 이면 모른다는 뜻이다.
	 * 비-심리스 ServerTravel 은 클라이언트를 끊었다 다시 붙이므로 서버가 스스로 알 방법이 없다.
	 */
	int32 ExpectedPlayers = 0;

	/** 마지막 접속 시각(월드 시간). ExpectedPlayers 를 모를 때의 폴백 판정에 쓴다 */
	float LastLoginTime = 0.f;

	/**
	 * 접속 대기 창이 열렸는가.
	 * PostLogin 은 HandleMatchHasStarted 보다 먼저 올 수 있어(호스트), 이게 없으면
	 * 첫 PostLogin 이 아직 0 인 StartDeadline 을 이미 지났다고 본다.
	 */
	bool bStartWindowOpen = false;

	/** 준비 시간이 시작됐는가. 접속 대기 폴링을 멈추는 기준 */
	bool bStarted = false;

	/**
	 * 매치를 이미 끝냈는가.
	 * 전원 확인과 시간 만료가 같은 프레임에 겹칠 수 있고, 재정의된 FinishMatch 가
	 * ServerTravel 을 두 번 부르면 전환이 꼬인다.
	 */
	bool bFinished = false;

	/** 탈출 판정이 다음 틱에 예약돼 있는가. 한 틱에 계기가 둘이어도 판정은 한 번이면 된다 */
	bool bEscapeCheckQueued = false;
};
