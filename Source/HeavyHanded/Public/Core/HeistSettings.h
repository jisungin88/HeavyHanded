#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Core/HeistOutcome.h"          // EHeistOutcome — UPROPERTY 노출 enum 이라 전방 선언 불가
#include "HeistSettings.generated.h"

/**
 * 코어 루프 밸런싱. Project Settings → Game → Heist.
 *
 * 소음 UNoiseSettings · 경계도 UAlertSettings 와 같은 패턴이다.
 * C++ / BP 어디에도 수치를 하드코딩하지 말 것 — 여기가 유일한 진리원이다.
 *
 * [여기 없는 값] 목표 금액과 본 작업 제한 시간은 장소마다 다르다
 *   (저택 $50,000 / 7분, 박물관 $120,000 / 8분, 은행 $250,000 / 9분 — 기획서 2장).
 *   그것들은 AHeistGameMode 의 EditDefaultsOnly 프로퍼티라 사이트별 BP 가 지정한다.
 *   여기 두면 저택과 은행이 같은 값을 공유해 버린다.
 */
UCLASS(config = HeistSystem, defaultconfig, meta = (DisplayName = "Heist"))
class HEAVYHANDED_API UHeistSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** CDO 라 절대 null 이 아니다 — 호출부에서 null 검사를 하지 말 것. UAlertSettings::Get() 주석 참고 */
	static const UHeistSettings* Get() { return GetDefault<UHeistSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ── 접속 대기 (Phase.Prep 이전) ──
	//
	// 리슨 서버라 호스트는 레벨이 열리는 즉시 들어와 있고 클라이언트는 로딩이 늦다.
	// 레벨이 열리자마자 준비 시간을 돌리면 나중에 들어온 팀원은 45초 중 20초만 받는다.
	//
	// 누구를 기다릴지는 URL 옵션 ?ExpectedPlayers=N 로 알려 준다 (은신처 → 저택 ServerTravel).
	// 그 값이 있으면 아래 두 수치는 안전망일 뿐이다.

	/**
	 * ExpectedPlayers 를 모를 때만 쓰는 폴백 — 마지막 접속 후 이만큼 조용하면 시작한다.
	 *
	 * [한계] 몇 명이 올지 모르는 채로 세는 시간이라, 3명이 들어온 뒤 4번째가
	 *   이 시간보다 늦으면 두고 출발한다. 실제 매치에서는 ExpectedPlayers 를 넘겨
	 *   이 경로로 떨어지지 않게 할 것 — 그때 이 값은 쓰이지 않는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Start", meta = (ClampMin = "0.0", Units = "s"))
	float PlayerJoinQuietSeconds = 3.f;

	/**
	 * 접속 대기 상한. 인원이 덜 찼어도 이 시간이 지나면 시작한다.
	 *
	 * 한 명이 로딩에서 영영 안 돌아왔을 때 나머지 셋이 붙잡혀 있지 않게 하는 장치다.
	 * 정상적인 레벨 로딩 시간보다 넉넉해야 한다 — 짧으면 이 안전망이 정상 대기를 잘라 버린다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Start", meta = (ClampMin = "0.0", Units = "s"))
	float PlayerJoinTimeoutSeconds = 30.f;

	// ── 페이즈 길이 ──

	/**
	 * 준비 시간 (기획서 2장). 역할 선택 · 드론 사전 정찰.
	 * 이 시간은 미션 제한 시간에 포함되지 않는다 — 미션 타이머는 Heist 진입부터 센다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Phase", meta = (ClampMin = "0.0", Units = "s"))
	float PrepSeconds = 45.f;

	/**
	 * 도주 시간 (기획서 2장). 경보 100% 또는 제한 시간 만료로 진입한다.
	 * 장소와 무관하게 90초 고정이라 여기 둔다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Phase", meta = (ClampMin = "0.0", Units = "s"))
	float EscapeSeconds = 90.f;

	// ── 밴 적재 ──

	/**
	 * 노획물이 적재존 안에 이만큼 머물러 있으면 적재로 확정한다.
	 *
	 * 오버랩 즉시 확정하지 않는 이유는 노획물의 물리가 살아 있기 때문이다. 던져 넣은 물건은
	 * 화물칸 바닥에서 튕겨 다시 굴러 나올 수 있고, 그것까지 적재로 세면 밴 쪽으로 던지기만 해도
	 * 돈이 들어온다. 이 시간은 "제대로 들어갔는가" 를 보는 유예다.
	 *
	 * 운반자가 들고 있는 동안은 세지 않는다 — 손에서 떠난 순간부터가 시작이다.
	 * 너무 길면 도주 중에 답답하고, 너무 짧으면 물리를 살려 둔 의미가 없어진다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Van", meta = (ClampMin = "0.0", Units = "s"))
	float LoadDwellSeconds = 1.5f;

	// ── 정산 ──

	/**
	 * 적재 금액을 팀 공용 골드로 넘기는 최소 등급.
	 *
	 * 기본값은 Success — 목표를 채우고 전원이 무사히 빠져나온 판에서만 지급한다.
	 *
	 * [값으로 뺀 이유] "성공했을 때만 준다" 와 3단계 등급이 만나면 경계가 하나 애매하다.
	 *   목표는 채웠는데 한 명이 잡힌 판(Partial)이 한 푼도 못 받는 것이 맞는지는
	 *   플레이해 봐야 안다. 코드에 박아 두면 그때 코드를 고쳐야 하고, 여기 있으면
	 *   Partial 로 한 칸 내리는 것으로 끝난다.
	 *
	 * 등급은 Failure < Partial < Success 순서라 이 값 이상이면 지급한다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Payout")
	EHeistOutcome MinOutcomeForPayout = EHeistOutcome::Success;
};
