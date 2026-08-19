#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameplayTagContainer.h"        // FGameplayTag 를 값으로 주고받는다
#include "Core/HeistPhase.h"             // EHeistPhaseReason — 인자 타입
#include "Noise/NoiseTypes.h"            // EAlertLevel — 다이내믹 델리게이트 시그니처라 전방 선언 불가
#include "HeistGameMode.generated.h"

struct FHeistStartConditions;

/**
 * 작업 레벨의 상태머신 구동부. Prep → Heist → Escape → Result.
 *
 * [무엇을 갖고 무엇을 안 갖는가]
 *   - 전이 규칙(다음이 무엇인가)  → HeistPhase 네임스페이스. GameMode 없이도 물어볼 수 있어야 한다
 *   - 접속 대기 판정(시작해도 되나) → HeistStartGate. 순수 함수라 테스트로 검증한다
 *   - 현재 상태와 복제              → AHeistGameState
 *   - 여기 남는 것                  → 타이머 · 로그 · 다른 시스템 호출 같은 부수효과뿐이다
 *
 *   규칙을 밖으로 뺀 이유는 재사용이 아니라 검증 가능성이다. 페이즈 전이와 접속 대기는
 *   틀려도 크래시가 나지 않고 "가끔 이상하더라" 로만 드러나서, 사람이 눈으로 잡을 수 없다.
 *
 * [로비 · 은신처는 여기 없다] Phase.Lobby / Phase.Hideout 은 레벨 자체가 다르고
 *   전환 수단이 ServerTravel 이다 (세션 파트). 이 GameMode 는 저택에 도착한 뒤부터 돈다.
 */
UCLASS()
class HEAVYHANDED_API AHeistGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AHeistGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/**
	 * 페이즈를 즉시 다음으로 넘긴다. 접속 대기 중이면 기다리지 않고 준비 시간을 시작한다.
	 *
	 * 치트(hh.Phase.Next)의 진입점이다. 사유가 Cheat 로 기록되므로 결과 화면 집계에서
	 * "정상적으로 끝난 판" 과 구분할 수 있다.
	 */
	void AdvancePhase(EHeistPhaseReason Reason);

protected:
	virtual void HandleMatchHasStarted() override;

	/**
	 * 페이즈에 들어간 직후, 다른 시스템에 걸어야 하는 것들.
	 *
	 * 페이즈 진입 부수효과는 앞으로 늘어난다 (Escape 진입 시 경비 증원, Result 진입 시 집계 확정).
	 * EnterPhase 본문에 조건문으로 쌓지 않고 여기 한 곳에 모은다.
	 */
	virtual void OnPhaseEntered(const FGameplayTag& Phase, EHeistPhaseReason Reason);

	/**
	 * 경계 단계가 바뀌었다. 경보(래치)면 본 작업을 즉시 끝내고 도주로 넘긴다.
	 *
	 * [본 작업 중일 때만이다] 준비 시간의 경보는 무시한다 — 어차피 Heist 진입에서
	 *   ResetAlert 로 지워지므로, 그것 때문에 도주로 넘기면 작업을 시작도 못 하고 끝난다.
	 *   이미 도주 · 결과라면 늦었다.
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
	 *
	 * [여기서 레벨을 옮기지 않는다] 기본 구현은 로그만 남긴다. ServerTravel 은 세션 파트
	 *   소관이고, 어디로 돌아갈지(로비 · 은신처)는 그쪽 흐름이 정한다.
	 *   `BP_HeistGameMode` 에서 이 함수를 재정의해 실제 전환을 붙이면 된다 —
	 *   AVanZone::HandleConfirmedLoot 과 같은 방식이다.
	 *
	 * [언제 불리는가] 둘 중 먼저 오는 쪽이다.
	 *   - 남아 있는 전원이 결과를 확인했다
	 *   - 결과 체류 시간(UHeistSettings::ResultSeconds)이 다 됐다
	 *
	 *   시간 쪽이 안전망이다. 한 명이 자리를 비웠다고 나머지가 영영 갇혀 있으면 안 된다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Heist")
	void FinishMatch();
	virtual void FinishMatch_Implementation();

	/**
	 * 이 장소의 목표 금액($). 기획서 2장 — 저택 $50,000 / 박물관 $120,000 / 은행 $250,000.
	 * 장소마다 다르므로 UHeistSettings 가 아니라 여기 있다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Heist", meta = (ClampMin = "0"))
	int32 TargetValue = 50000;

	/**
	 * 본 작업(Phase.Heist) 제한 시간. 기획서 2장 — 저택 7분 / 박물관 8분 / 은행 9분.
	 * 준비 시간은 여기 포함되지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Heist", meta = (ClampMin = "1.0", Units = "s"))
	float HeistSeconds = 420.f;

	/**
	 * 이 작업 레벨이 어느 장소인가 (Site.*). 기획서 2장 — 저택 · 박물관 · 은행.
	 *
	 * 캠페인 진행(3개 장소 통과 = 최종 성공)을 기록하는 키다. 비워 두면 이 판을 성공해도
	 * 진행이 올라가지 않으므로, 결과 확정에서 경고를 남긴다.
	 *
	 * 기본값을 주지 않는다 — 저택 값을 박아 두면 박물관 BP 가 지정을 잊었을 때
	 * 조용히 저택을 두 번 통과한 것으로 기록된다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Heist")
	FGameplayTag SiteTag;

private:
	/**
	 * 이 판에 몇 명이 올 예정인지 알아낸다. 모르면 0.
	 *
	 * 값의 출처가 바뀌어도(URL → GameInstance) 나머지 코드가 흔들리지 않게 여기 하나로 모은다.
	 * 우선순위와 각 경로의 한계는 구현부 주석에 있다.
	 */
	int32 ResolveExpectedPlayers(const FString& Options) const;

	/**
	 * 지금 상황을 판정용 값으로 옮긴다. 시각을 경과·잔여로 바꾸는 곳이 여기다.
	 * (const 가 아닌 이유는 AGameMode::GetNumPlayers() 가 non-const 이기 때문이다)
	 */
	FHeistStartConditions MakeStartConditions();

	/** 주기적으로 HeistStartGate 에 물어보고, 답에 따라 시작하거나 계속 기다린다 */
	void TickStartWait();

	/** 접속 대기를 끝내고 Phase.Prep 으로 들어간다 */
	void StartPrep();

	/** 페이즈를 바꾸고 다음 전환을 예약한다 */
	void EnterPhase(const FGameplayTag& Phase, EHeistPhaseReason Reason);

	/** 페이즈 타이머 만료 — 시간 만료 경로의 전이 */
	void HandlePhaseElapsed();

	/** 페이즈별 지속 시간(초). 0 이면 카운트다운 없이 머문다 */
	float GetPhaseDuration(const FGameplayTag& Phase) const;

	/**
	 * 탈출 판정을 다음 틱에 예약한다.
	 *
	 * [왜 그 자리에서 판정하지 않는가] 판정 계기가 전부 '명단이 바뀌는 순간' 이라,
	 *   즉시 판정하면 명단을 바꾸던 코드가 아직 안 끝난 상태에서 결과 화면이 열린다.
	 *   실제로 그랬다 — 마지막에 탄 사람의 Event.Player.BoardedVan 이 Result 진입 뒤에
	 *   발송되고, 승차 로그도 결과 로그 뒤에 찍혔다.
	 *
	 *   한 틱 미루면 계기를 만든 코드가 전부 끝난 뒤에 판정한다. 접속 종료 경로가 원래
	 *   이렇게 하고 있었는데(PlayerArray 가 아직 안 줄어서), 나머지 둘도 같은 이유가 있었다.
	 *
	 * 같은 틱에 여러 번 불려도 예약은 하나다.
	 */
	void RequestEscapeCheck();

	/**
	 * 생존자가 전부 밴에 탔으면 결과로 넘긴다. 직접 부르지 말고 RequestEscapeCheck() 를 쓸 것.
	 *
	 * [언제 성립하는가] 계기가 셋이다 — 누가 타거나, 누가 다운되거나
	 *   (남은 생존자가 줄어 이미 탄 사람들만 남는다), 누가 접속을 끊거나.
	 *   조건이 아니라 계기가 셋인 것이므로 판정은 한 곳이어야 한다.
	 *
	 * [준비 시간은 제외한다] 플레이어는 밴 근처에서 시작한다. Prep 부터 세면 스폰 직후
	 *   전원이 볼륨 안에 있어서 $0 으로 판이 끝난다. 본 작업에 들어가야 승차를 센다.
	 */
	void TryFinishByEscape();

	/** 결과 진입 시 체포를 확정한다. 미승차자와 다운자가 대상이다 */
	void ResolveArrests();

	/**
	 * 적재 금액을 팀 공용 골드로 넘긴다. (결과 등급이 확정된 뒤)
	 *
	 * 지급 기준은 `UHeistSettings::MinOutcomeForPayout` — 기본값은 부분 성공 이상이다.
	 * 여기서 넘긴 값이 은신처 정산과 장비 구매의 입력이 된다.
	 */
	void PayoutTeamGold();

	/**
	 * 이 장소를 통과했으면 런 진행에 기록한다. (결과 등급이 확정된 뒤)
	 *
	 * 통과 기준은 등급 `Success` 뿐이다 — 기획서 2장의 작업 성공(목표 금액 달성 + 최소 1인
	 * 승차)과 같은 선이고, 그 아래는 "실패한 장소를 처음부터 재시작" 대상이다.
	 * 목표를 못 채운 판(Partial)은 실어 온 돈은 받아 가지만 장소는 다시 해야 한다.
	 *
	 * [여기서 하지 않는 것] 통과 뒤에 어디로 가는가(은신처 · 최종 성공 연출)는 정하지 않는다.
	 *   사실만 남기고 이동은 FinishMatch 재정의(세션 파트)가 정한다.
	 */
	void RecordSiteProgress();

	/**
	 * 체포된 사람을 런 진행으로 넘긴다. (체포가 확정된 뒤)
	 *
	 * 이걸 안 하면 잡힌 사람이 결과 화면과 함께 사라진다 — 레벨이 바뀌면 GameState 도
	 * PlayerState 도 전부 새로 생기기 때문이다. 은신처의 '팀원 구출' 이 돌아가려면
	 * 그 사실이 레벨을 건너야 한다.
	 *
	 * 등급이나 지급 여부와 무관하게 항상 넘긴다. 잡힌 것은 잡힌 것이다.
	 */
	void CarryOverArrests();

	/**
	 * 이 플레이어의 다운 상태 변화를 구독한다.
	 *
	 * 다운은 승차만큼이나 확실한 종료 계기다 — 마지막 한 명이 밖에서 쓰러지면 그 순간
	 * 생존자가 이미 밴에 다 타 있는 상태가 된다. 구독이 없으면 그때 판이 안 끝나고
	 * 도주 시간을 끝까지 기다리게 된다.
	 *
	 * 다운 시스템(전영배)이 아직 태그를 붙이지 않아 지금은 한 번도 불리지 않는다.
	 */
	void WatchDownedState(APlayerState* Player);

	/** 다운 태그가 붙거나 떨어졌다 */
	void HandleDownedTagChanged(const FGameplayTag Tag, int32 NewCount);

	FTimerHandle PhaseTimerHandle;
	FTimerHandle StartWaitHandle;

	/** 접속 대기 상한 시각(월드 시간). 인원이 덜 차도 이때는 시작한다 */
	float StartDeadline = 0.f;

	/**
	 * 이 판에 올 인원. 0 이면 모른다는 뜻이다.
	 *
	 * 서버가 스스로 알아낼 방법이 없다. 비-심리스 ServerTravel 은 클라이언트를 끊었다가
	 * 다시 붙이기 때문에, 새 레벨의 GameMode 는 이전 로비에 몇 명이 있었는지 기억하지 못한다.
	 */
	int32 ExpectedPlayers = 0;

	/** 마지막 접속 시각(월드 시간). ExpectedPlayers 를 모를 때의 폴백 판정에 쓴다 */
	float LastLoginTime = 0.f;

	/**
	 * 접속 대기 창이 열렸는가.
	 *
	 * PostLogin 은 HandleMatchHasStarted 보다 먼저 올 수 있다 (호스트가 그렇다).
	 * 이 플래그가 없으면 그 첫 PostLogin 이 아직 0 인 StartDeadline 을 이미 지났다고 본다.
	 */
	bool bStartWindowOpen = false;

	/** 준비 시간이 시작됐는가. 접속 대기 폴링을 멈추는 기준 */
	bool bStarted = false;

	/**
	 * 매치를 이미 끝냈는가.
	 *
	 * 전원 확인과 시간 만료가 같은 프레임에 겹칠 수 있고, 재정의된 FinishMatch 가
	 * ServerTravel 을 부르는데 그것이 두 번 불리면 전환이 꼬인다.
	 */
	bool bFinished = false;

	/**
	 * 탈출 판정이 다음 틱에 예약돼 있는가.
	 *
	 * 한 틱에 둘이 동시에 타면 계기가 두 번 오는데, 판정은 한 번이면 된다.
	 * (두 번 돌아도 결과는 같지만, 예약이 쌓이는 구조를 남겨 둘 이유가 없다)
	 */
	bool bEscapeCheckQueued = false;
};
