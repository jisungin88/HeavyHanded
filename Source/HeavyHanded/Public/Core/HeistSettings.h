#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"       // FGameplayTag 를 값으로 보유 — 전방 선언 불가
#include "UObject/SoftObjectPtr.h"      // TSoftObjectPtr 를 값으로 보유
#include "UObject/SoftObjectPath.h"     // FSoftObjectPath 를 값으로 반환
#include "Core/HeistOutcome.h"          // EHeistOutcome — UPROPERTY 노출 enum 이라 전방 선언 불가
#include "Core/Spectate/HeistSpectateTypes.h"
#include "HeistSettings.generated.h"

class UWorld;

/**
 * 장소(Site.*) 하나와 그 작업 레벨.
 * TMap 이 아닌 것은 FGameplayTag 키가 `.ini` 에서 손으로 못 고칠 줄이 되기 때문이고,
 * DataTable 이 아닌 것은 밸런싱이 아니라 경로 배선이고 `.uasset` 은 병합이 안 되기 때문이다.
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
	 * 이 장소의 작업 레벨. 소프트 참조인 것은 Settings 가 모듈 로드 시점에 만들어져서,
	 * 하드 참조면 그때 레벨이 통째로 로드되기 때문이다. 경로만 꺼내 ServerTravel 에 넘긴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Travel",
		meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> Level;
};

/**
 * 코어 루프 밸런싱. Project Settings → Game → Heist.
 * **C++ / BP 어디에도 수치를 하드코딩하지 말 것** — 여기가 유일한 진리원이다.
 * 목표 금액과 제한 시간은 장소마다 달라 여기 없다 (사이트별 GameMode BP 가 갖는다).
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
	 * 안전한 기본값을 두지 않는다. "모르겠으면 저택" 은 어떤 상황에서도 정답이 아니다.
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

	/**
	 * 판이 끝나면 돌아갈 은신처 레벨. SiteLevels 표에 넣지 않는 것은 은신처가 장소가 아니라서다 —
	 * 넣으면 목표 선택 UI 에 섞여 나오고 캠페인 통과 판정도 그것을 센다.
	 * 비어 있으면 떠나지 않고 결과 화면에 머문다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Travel")
	TSoftObjectPtr<UWorld> HideoutLevel;

	/** 은신처 레벨 경로. 지정돼 있지 않으면 빈 경로다 — 호출부가 이동을 막아야 한다 */
	FSoftObjectPath GetHideoutLevel() const
	{
		return HideoutLevel.ToSoftObjectPath();
	}

	// ── 접속 대기 (Phase.Prep 이전) ──
	// 리슨 서버라 호스트는 즉시 들어와 있고 클라이언트는 로딩이 늦다. 바로 시작하면
	// 나중에 들어온 팀원은 45초 중 20초만 받는다. 인원은 ?ExpectedPlayers 로 알려 준다

	/**
	 * ExpectedPlayers 를 모를 때만 쓰는 폴백 — 마지막 접속 후 이만큼 조용하면 시작한다.
	 * 몇 명이 올지 모르는 채로 세는 시간이라 **늦은 사람을 두고 출발할 수 있다.**
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
	 * 결과 화면 체류 시간(초). 전원이 확인을 누르면 그전에도 넘어가므로 이 값은 안전망이다.
	 * **0 이면 전원 확인만 기다린다** — HUD 확인 버튼이 붙기 전에는 그렇게 두지 말 것.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Phase", meta = (ClampMin = "0.0", Units = "s"))
	float ResultSeconds = 30.f;

	// ── 밴 적재 ──

	/**
	 * 노획물이 적재존 안에 이만큼 머물면 적재로 확정한다. 물리가 살아 있어서 던져 넣은 물건이
	 * 튕겨 나올 수 있고, 즉시 확정하면 밴 쪽으로 던지기만 해도 돈이 들어온다.
	 * 운반자가 들고 있는 동안은 세지 않는다 — 손에서 떠난 순간부터가 시작이다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Van", meta = (ClampMin = "0.0", Units = "s"))
	float LoadDwellSeconds = 1.5f;

	// ── 정산 ──

	/**
	 * 적재 금액을 팀 골드로 넘기는 최소 등급. 등급 순서가 Failure < Partial < Success 라
	 * 이 값 이상이면 지급한다. 기본값 Partial — **한 명이라도 태우고 떠났으면 돈은 들어온다.**
	 * 밸런싱에서 가장 먼저 흔들릴 자리라 값으로 남겨 뒀다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Payout")
	EHeistOutcome MinOutcomeForPayout = EHeistOutcome::Partial;

	// -- 관전

	/** 관전자에게 보여줄 정보의 범위 */
	UPROPERTY(config, EditAnywhere, Category = "Spectate")
	EHeistSpectateInfoLevel SpectateInfoLevel = EHeistSpectateInfoLevel::FollowTarget;
};
