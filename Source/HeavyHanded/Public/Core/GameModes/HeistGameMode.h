#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameplayTagContainer.h"        // FGameplayTag 를 값으로 주고받는다
#include "Core/HeistPhase.h"             // EHeistPhaseReason — 인자 타입
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
};
