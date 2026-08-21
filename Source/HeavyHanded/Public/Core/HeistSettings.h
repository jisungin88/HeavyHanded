#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"       // FGameplayTag 를 값으로 보유 — 전방 선언 불가
#include "UObject/SoftObjectPtr.h"      // TSoftObjectPtr 를 값으로 보유
#include "UObject/SoftObjectPath.h"     // FSoftObjectPath 를 값으로 반환
#include "Core/HeistOutcome.h"          // EHeistOutcome — UPROPERTY 노출 enum 이라 전방 선언 불가
#include "HeistSettings.generated.h"

class UWorld;

/**
 * 장소(Site.*) 하나와 그 작업 레벨.
 *
 * [왜 TMap 이 아니라 구조체 배열인가]
 *   TMap 의 키가 FGameplayTag 면 `.ini` 직렬화가 `((TagName="Site.Mansion"), ...)` 형태가 되어
 *   손으로 고칠 수 없는 줄이 된다. 필드 이름이 남는 구조체 배열은 그대로 읽고 고칠 수 있다.
 *   장소는 셋뿐이라 선형 조회의 비용도 논할 것이 없다.
 *
 * [왜 DataTable 이 아닌가]
 *   행이 여럿이니 문서 04 의 기준으로는 DataTable 쪽에 가깝다. 그래도 여기 둔 이유가 둘 있다 —
 *     1. 밸런싱 수치가 아니라 **경로 배선**이다. 기획이 플레이테스트 중에 만질 값이 아니다
 *     2. `.uasset` 은 병합이 안 된다. 이 표는 레벨을 만드는 사람마다 한 줄씩 더하게 되는데,
 *        그걸 DataTable 로 두면 두 사람이 같은 날 장소를 추가할 때 한쪽이 소실된다 (문서 06)
 *   장소별 밸런싱(목표 금액 · 제한 시간)은 여전히 사이트별 GameMode BP 가 갖는다.
 */
USTRUCT()
struct FHeistSiteLevel
{
	GENERATED_BODY()

	// 필드에는 config 를 달지 않는다 — 저장 단위는 바깥의 SiteLevels 배열이고,
	// 구조체는 통째로 직렬화된다. 여기 달아 봐야 아무 뜻이 없다

	/** 이 장소의 식별자(Site.*). Config/Tags/Phase.ini 에 등록된 것만 의미가 있다 */
	UPROPERTY(EditAnywhere, Category = "Travel")
	FGameplayTag SiteTag;

	/**
	 * 이 장소의 작업 레벨.
	 *
	 * 소프트 참조인 이유는 UNoiseSettings::NoiseProfiles 와 같다 — Settings 는 모듈 로드 시점에
	 * 만들어지고, 하드 참조로 두면 그때 레벨이 통째로 로드된다.
	 * 여기서는 로드하지 않는다. 경로만 꺼내 ServerTravel 에 넘긴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Travel",
		meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> Level;
};

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

	// ── 장소 이동 ──
	//
	// 기획서 2장 — 은신처에서 다음 목표를 고르고 출발한다. 그 '출발' 이 어느 맵을 여는가가
	// 이 표다. URunProgressSubsystem::TryDepartToSite 가 유일한 소비자다.

	/**
	 * 장소(Site.*) → 작업 레벨. **새 장소 맵을 만들면 여기 한 줄을 더한다.**
	 *
	 * 등록되지 않은 장소로 출발을 시도하면 떠나지 않고 경고를 남긴다 — 엉뚱한 맵을 열지 않는다.
	 * 같은 태그를 두 번 넣으면 앞선 것이 이긴다. 막지는 않으니 넣지 말 것.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Travel", meta = (TitleProperty = "SiteTag"))
	TArray<FHeistSiteLevel> SiteLevels;

	/**
	 * 이 장소의 레벨 경로. **등록돼 있지 않으면 빈 경로다** — 호출부가 출발을 막아야 한다.
	 *
	 * 배열을 직접 노출하지 않고 래퍼를 두는 것은 UNoiseSettings::GetSurfaceCoeff 와 같은 이유다.
	 * 다만 여기서는 안전한 기본값이 없다 — "모르겠으면 저택" 은 어떤 상황에서도 정답이 아니다.
	 * 없는 것은 없다고 답하고, 그 판단은 부르는 쪽이 한다.
	 */
	FSoftObjectPath GetSiteLevel(const FGameplayTag& SiteTag) const
	{
		if (!SiteTag.IsValid())
		{
			return FSoftObjectPath();
		}

		for (const FHeistSiteLevel& Entry : SiteLevels)
		{
			if (Entry.SiteTag == SiteTag)
			{
				return Entry.Level.ToSoftObjectPath();
			}
		}

		return FSoftObjectPath();
	}

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

	/**
	 * 결과 화면 체류 시간(초). 이 시간이 지나면 매치를 끝낸다.
	 *
	 * 전원이 확인을 누르면 그전에도 넘어간다 — 이 값은 **아무도 누르지 않았을 때의 안전망**이다.
	 * 결과 화면을 읽을 시간은 줘야 하지만, 한 명이 자리를 비웠다고 나머지가
	 * 영영 갇혀 있으면 안 된다.
	 *
	 * 0 이면 카운트다운 없이 전원 확인만 기다린다 — HUD 확인 버튼이 붙기 전에는
	 * 그렇게 두지 말 것. 나갈 방법이 없어진다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Phase", meta = (ClampMin = "0.0", Units = "s"))
	float ResultSeconds = 30.f;

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
	 * 기본값은 Partial — **밴이 한 명이라도 태우고 떠났으면 실어 온 돈은 들어온다** (기획 확정).
	 *   목표 금액을 못 채운 판도 마찬가지다. 목표 미달의 대가는 등급(Partial)과
	 *   장소 재도전이지 무보수가 아니고, 미승차자의 대가는 체포다.
	 *
	 *   Failure 는 전멸뿐이라, 이 기본값에서 지급이 빠지는 판은 아무도 돌아오지 못한 판 하나다.
	 *
	 * [값으로 남겨 둔 이유] 밸런싱에서 가장 먼저 흔들릴 자리다. 목표 미달 판이 전액을
	 *   가져가는 것이 후반 장소($250,000)에서도 성립하는지는 플레이해 봐야 안다.
	 *   Success 로 한 칸 올리면 "목표를 채운 판에서만 지급" 이 된다.
	 *
	 * 등급은 Failure < Partial < Success 순서라 이 값 이상이면 지급한다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Payout")
	EHeistOutcome MinOutcomeForPayout = EHeistOutcome::Partial;
};
