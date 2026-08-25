#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"   // FUniqueNetIdRepl — 값으로 보유
#include "GameplayTagContainer.h"              // FGameplayTag — 값으로 보유
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunProgressSubsystem.generated.h"

/**
 * 한 판(run)의 진행 상황. 레벨을 건너 살아남아야 하는 것만 들어온다 —
 * 팀 골드 · 역할 · 참가자 명단 · 구매 장비 · 체포자 · 통과한 장소.
 *
 * 수명이 둘이라 초기화 함수도 둘이다. BeginNewRun() 은 런 단위(명단 · 장비 · 진입점)만
 * 비우고, ResetCampaign() 은 골드와 역할까지 전부 비운다. 합치면 판마다 골드가 날아간다.
 *
 * **복제되지 않는다.** 서브시스템은 프로세스마다 하나씩이고 서버 것과 클라이언트 것이
 * 다른 객체다 — 클라이언트가 화면에 그려야 하는 값이면 GameState 로 옮겨 복제할 것.
 * 그래서 쓰기는 전부 서버 전용이다.
 */
UCLASS()
class HEAVYHANDED_API URunProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 어디서든 이걸로 접근한다. GameInstance 가 없으면(에디터 툴 등) nullptr */
	UFUNCTION(BlueprintPure, Category = "Run", meta = (WorldContext = "WorldContext"))
	static URunProgressSubsystem* Get(const UObject* WorldContext);

	// ── 팀 공용 골드 ──
	//
	// 개인 지갑이 아니다. 적재 금액은 전부 한 지갑으로 들어가고 은신처에서 함께 쓴다.

	/** 팀 공용 잔액($) */
	UFUNCTION(BlueprintPure, Category = "Run|Gold")
	int32 GetTeamGold() const { return TeamGold; }

	/** 잔액을 더한다(음수면 차감). 결과가 음수면 0 으로 잘린다. (서버 전용) */
	void AddTeamGold(int32 DeltaGold);

	/**
	 * 지출을 시도한다. 잔액이 모자라면 false 이고 **아무것도 차감하지 않는다.** (서버 전용)
	 * 확인과 차감을 호출부에서 나눠 하면 두 명이 같은 프레임에 살 때 잔액이 음수로 내려간다.
	 */
	bool TrySpendTeamGold(int32 Cost);

	// ── 참가자 명단 ──

	/**
	 * 로비에서 확정된 참가자를 등록한다. (세션 파트가 호출)
	 * 신원까지 들고 있는 것은 비-심리스 ServerTravel 뒤에 PlayerState 가 전부 새로 생기기
	 * 때문이다 — 포인터나 접속 순서를 키로 쓰면 레벨을 넘는 순간 어긋난다.
	 */
	void SetConfirmedRoster(const TArray<FUniqueNetIdRepl>& InRoster);

	/** 이 판에 참가하기로 한 인원. 아직 확정 전이면 0 */
	UFUNCTION(BlueprintPure, Category = "Run|Roster")
	int32 GetRosterNum() const { return ConfirmedRoster.Num(); }

	/** 이 플레이어가 확정 명단에 있는가 */
	bool IsInRoster(const FUniqueNetIdRepl& PlayerId) const;

	// ── 역할 선택 ──
	//
	// 은신처에서 고르고 저택에서 쓴다. 한 번 정하면 바뀌지 않는다.

	/**
	 * 역할을 정한다. 이미 골랐거나 남이 가져간 역할이면 false 이고 아무것도 바꾸지 않는다. (서버 전용)
	 * Set 이 아니라 Try 인 것은 판정과 반영이 갈라지면 두 사람이 같은 역할을 동시에 통과하기 때문이다.
	 */
	bool TrySelectRole(const FUniqueNetIdRepl& PlayerId, const FGameplayTag& RoleTag);

	/** 이 플레이어가 고른 역할. 아직 안 골랐으면 무효 태그 */
	FGameplayTag GetSelectedRole(const FUniqueNetIdRepl& PlayerId) const;

	/** 이 역할을 이미 누가 가져갔는가. UI 가 선택 목록을 흐리게 하는 데 쓴다 */
	bool IsRoleTaken(const FGameplayTag& RoleTag) const;

	// ── 진입점 선택 ──
	//
	// 팀 전체가 한 곳에서 시작하므로 키가 없는 값 하나다. 런 단위 — BeginNewRun 이 지운다.

	/**
	 * 진입점을 정한다. 역할과 달리 **다시 고를 수 있다.** 무효 태그면 false. (서버 전용)
	 * 그 레벨에 실제로 있는지는 여기서 검사하지 않는다 — 은신처에는 그 레벨이 로드돼 있지 않고,
	 * 도착한 뒤 HeistEntryGate 가 판정한다.
	 */
	bool TrySelectEntry(const FGameplayTag& EntryTag);

	/** 팀이 고른 진입점(Entry.*). 아직 안 골랐으면 무효 태그 */
	UFUNCTION(BlueprintPure, Category = "Run|Entry")
	FGameplayTag GetSelectedEntry() const { return SelectedEntry; }

	/** 선택을 되돌린다. (서버 전용) 레벨의 기본 진입점에서 시작하게 된다 */
	void ClearSelectedEntry();

	// ── 구매 장비 ──
	//
	// 목록만 작업 레벨까지 건너가고, 아이템 액터로 스폰되는 순간 소비된다.
	// 컨테이너가 아니라 수량 맵인 것은 같은 장비를 여러 개 살 수 있기 때문이다.

	/** 구매 목록에 추가한다. 골드 차감은 호출부가 TrySpendTeamGold 로 따로 한다. (서버 전용) */
	void AddPurchasedEquipment(const FGameplayTag& EquipmentTag, int32 Count = 1);

	/** 이 장비를 몇 개 샀는가 */
	UFUNCTION(BlueprintPure, Category = "Run|Equipment")
	int32 GetPurchasedCount(FGameplayTag EquipmentTag) const;

	/** 구매 목록 전체. 작업 레벨의 스폰 구역이 이걸 훑어 아이템을 만든다 */
	const TMap<FGameplayTag, int32>& GetPurchasedEquipment() const { return PurchasedEquipment; }

	/**
	 * 구매 목록을 비운다. 스폰이 끝난 뒤 **바로 이어서** 부를 것. (서버 전용)
	 * 갈라 두면 레벨이 두 번 로드될 때(재접속 · 리스타트) 같은 장비가 두 번 나온다.
	 */
	void ConsumePurchasedEquipment();

	// ── 체포된 팀원 ──
	//
	// 캠페인 단위 — BeginNewRun 이 지우지 않는다. 구출 전까지 남아야 하는 사실이다.
	// 다음 판에 나갈 수 있는가는 여기서 답하지 않는다. 사실만 들고 있는다.

	/**
	 * 이번 판에서 잡힌 사람들을 기록한다. (서버 전용 — 결과 확정 시)
	 * 이미 잡혀 있던 사람은 다시 넣지 않는다. 두 판 연속 잡혀도 구출은 한 번이다.
	 */
	void RecordArrested(const TArray<FUniqueNetIdRepl>& ArrestedIds);

	/** 이 플레이어가 잡혀 있는가 */
	bool IsArrested(const FUniqueNetIdRepl& PlayerId) const;

	/** 잡혀 있는 인원 */
	UFUNCTION(BlueprintPure, Category = "Run|Arrest")
	int32 GetArrestedNum() const { return ArrestedPlayers.Num(); }

	/** 잡혀 있는 사람들. 은신처 구출 UI 가 이 목록을 그린다 */
	const TArray<FUniqueNetIdRepl>& GetArrestedPlayers() const { return ArrestedPlayers; }

	/**
	 * 팀 골드를 내고 구출한다. 잡혀 있지 않거나 잔액이 모자라면 false 이고 아무것도 바꾸지 않는다.
	 * 비용을 인자로 받는 것은 그것이 밸런싱이라서다 — 규칙은 은신처가 안다. (서버 전용)
	 */
	bool TryRescue(const FUniqueNetIdRepl& PlayerId, int32 Cost);

	/**
	 * 관전으로 형기를 마친 사람들을 체포 명단에서 뺀다. (서버 전용 — 결과 확정 시)
	 * TryRescue 는 **형기를 건너뛰는** 값이고 이쪽은 다 산 결과라 별개 함수다.
	 * 잡혀 있지 않은 사람이 섞여 있어도 무시한다 — 호출부는 관전자 전원을 넘긴다.
	 */
	void ReleaseArrested(const TArray<FUniqueNetIdRepl>& PlayerIds);

	// ── 통과한 장소 ──
	//
	// 캠페인 단위 — BeginNewRun 이 지우지 않는다. "이 방에서 어디까지 왔는가" 이기 때문이다.
	// 통과한 뒤 어디로 가는가는 여기서 정하지 않는다 (아래 '출발' 과 혼동하지 말 것).

	/**
	 * 이 장소를 통과했다고 기록한다. (서버 전용 — 결과 확정 시) 같은 장소를 두 번 넣지 않는다.
	 * 무효 태그는 거부하고 경고한다 — 빈 태그가 들어가면 통과 수가 조용히 틀리고,
	 * 그 값이 최종 성공을 가른다.
	 */
	void RecordSiteCleared(const FGameplayTag& SiteTag);

	/** 이 장소를 통과했는가. 목표 선택 UI 가 이미 지난 곳을 가리는 데 쓴다 */
	UFUNCTION(BlueprintPure, Category = "Run|Progress")
	bool IsSiteCleared(FGameplayTag SiteTag) const;

	/** 통과한 장소 수. 최종 성공은 이 값이 3이 되는 것이다 */
	UFUNCTION(BlueprintPure, Category = "Run|Progress")
	int32 GetClearedSiteNum() const { return ClearedSites.Num(); }

	/** 통과한 장소들. 통과한 순서대로 쌓인다 */
	const TArray<FGameplayTag>& GetClearedSites() const { return ClearedSites; }

	// ── 출발 ──

	/**
	 * 이 장소로 출발한다. 전원을 데리고 ServerTravel 한다. SiteLevels 에 없으면 false. (서버 전용)
	 * 진입점을 안 골랐어도 떠나고, BeginNewRun 도 부르지 않는다 — 재도전 때 산 장비가 사라진다.
	 * **이 호출 뒤의 코드는 같은 월드에서 이어지지 않는다** (비-심리스 ServerTravel).
	 */
	UFUNCTION(BlueprintCallable, Category = "Run|Travel")
	bool TryDepartToSite(const FGameplayTag& SiteTag);

	// ── 수명 경계 ──

	/**
	 * 판 하나를 새로 시작한다. 런 단위 데이터(명단 · 구매 장비 · 진입점)를 비운다. (서버 전용)
	 * 팀 골드와 역할은 남는다 — 둘 다 캠페인 단위다.
	 */
	void BeginNewRun();

	/** 새 방(세션)을 연다. 팀 골드까지 포함해 전부 비운다. (서버 전용) */
	void ResetCampaign();

private:
	/** 쓰기 연산의 공통 관문. 서버가 아니면 경고를 남기고 막는다 */
	bool EnsureServerAuthority(const TCHAR* Operation) const;

	/** 팀 공용 잔액($) */
	UPROPERTY()
	int32 TeamGold = 0;

	/** 로비에서 확정된 참가자 */
	UPROPERTY()
	TArray<FUniqueNetIdRepl> ConfirmedRoster;

	/**
	 * 플레이어별 선택 역할. 캠페인 단위.
	 * UPROPERTY 가 아닌 것은 UObject 를 참조하지 않아 GC 가 볼 것이 없고,
	 * FUniqueNetIdRepl 을 TMap 키로 리플렉션에 올리면 UHT 제약에 걸리기 때문이다.
	 */
	TMap<FUniqueNetIdRepl, FGameplayTag> SelectedRoles;

	/** 은신처에서 산 장비와 수량. 런 단위 — 작업 레벨에서 스폰되며 소비된다 */
	TMap<FGameplayTag, int32> PurchasedEquipment;

	/** 팀이 고른 진입점(Entry.*). 런 단위. 무효 태그면 레벨 기본값으로 폴백한다 */
	UPROPERTY()
	FGameplayTag SelectedEntry;

	/** 잡혀 있는 팀원. 캠페인 단위. UPROPERTY 가 아닌 이유는 SelectedRoles 와 같다 */
	TArray<FUniqueNetIdRepl> ArrestedPlayers;

	/**
	 * 통과한 장소(Site.*). 캠페인 단위.
	 * 배열인 것은 통과 순서가 그대로 진행 경로이고 최대 3개라 TSet 의 이점이 없어서다.
	 */
	UPROPERTY()
	TArray<FGameplayTag> ClearedSites;
};
