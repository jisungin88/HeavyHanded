#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"   // FUniqueNetIdRepl — 값으로 보유
#include "GameplayTagContainer.h"              // FGameplayTag — 값으로 보유
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunProgressSubsystem.generated.h"

/**
 * 한 판(run)의 진행 상황. 레벨을 건너 살아남아야 하는 것만 들어온다.
 *
 * ┌─ 무엇을 여기 두는가 ────────────────────────────────────────────┐
 * │ 로비 → 은신처 → 저택 → 결과 를 건너 유지돼야 하는 것             │
 * │   팀 공용 골드 · 선택한 역할 · 참가자 명단 · 구매 장비            │
 * │                                                                  │
 * │ 여기 두지 않는 것                                                │
 * │   한 레벨 안에서만 쓰는 것 → GameState (페이즈 · 타이머 · 적재액) │
 * │   플레이어별 한 판 기록    → PlayerState (기여도 · 상태)          │
 * └──────────────────────────────────────────────────────────────────┘
 *
 * [수명이 두 가지다 — 이 구분이 이 클래스의 뼈대다]
 *
 *   캠페인 단위 (방이 유지되는 동안 계속)
 *     팀 골드   은신처가 판 사이의 허브라 누적된다
 *     역할      한 번 고르면 바뀌지 않는다 (기획 확정)
 *
 *   런 단위 (판 하나)
 *     참가자 명단   판마다 인원이 달라질 수 있다
 *     구매 장비     작업 레벨에서 아이템으로 스폰되며 그때 소비된다
 *
 *   그래서 초기화 함수가 둘이다.
 *     BeginNewRun()   판 하나를 새로 시작한다. 골드와 역할은 남는다
 *     ResetCampaign() 새 방을 연다. 전부 비운다
 *   하나로 합치면 판을 시작할 때마다 골드가 날아가고 역할이 풀린다.
 *
 * [왜 GameInstance 상속이 아니라 서브시스템인가]
 *   GameInstance 는 프로젝트당 하나뿐이라 세션 · 정산 · 설정 · 오디오가 전부 한 클래스로
 *   뭉친다. 서브시스템은 책임별로 나눌 수 있고, 클래스를 추가하는 것만으로 등록돼
 *   Project Settings 를 건드리지 않는다 — 팀 전체에 영향을 주는 변경이 없다.
 *
 * [반드시 알아야 할 것 1 — 이것은 복제되지 않는다]
 *   서브시스템에는 복제 기능이 없다. 여기 있는 값은 그것을 넣은 프로세스만 안다.
 *   클라이언트가 화면에 그려야 하는 값이면 GameMode 가 GameState 로 옮겨 복제할 것.
 *   여기서 직접 읽어 UI 를 그리면 호스트 화면에만 맞는 숫자가 뜬다.
 *
 * [반드시 알아야 할 것 2 — 서버 것과 클라이언트 것은 다른 객체다]
 *   각 프로세스에 하나씩 있다. 호스트가 넣은 값이 참가자에게 가지 않는다.
 *   그래서 쓰기는 서버에서만 허용한다. 클라이언트에서 부르면 무시하고 경고를 남긴다.
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
	// 개인 지갑이 아니다. 적재한 금액은 전부 한 지갑으로 들어가고 은신처에서 함께 쓴다.
	// 그래서 "누구의 돈인가" 를 따질 필요가 없고, 재접속 후 신원을 다시 이어붙이는
	// 문제도 골드에 대해서는 생기지 않는다.

	/** 팀 공용 잔액($) */
	UFUNCTION(BlueprintPure, Category = "Run|Gold")
	int32 GetTeamGold() const { return TeamGold; }

	/** 잔액을 더한다(음수면 차감). 결과가 음수면 0 으로 잘린다. (서버 전용) */
	void AddTeamGold(int32 DeltaGold);

	/**
	 * 지출을 시도한다. (서버 전용)
	 *
	 * @return 잔액이 모자라면 false 이고 **아무것도 차감하지 않는다**
	 *
	 * AddTeamGold(-Cost) 와 나눠 둔 이유는 "확인 후 차감" 을 호출부마다 다시 짜면
	 * 한 곳이라도 순서를 틀리는 순간 잔액이 음수로 내려가기 때문이다.
	 * 공용 지갑이라 여러 명이 같은 프레임에 살 수 있다 — 판정과 차감이 갈라지면 안 된다.
	 */
	bool TrySpendTeamGold(int32 Cost);

	// ── 참가자 명단 ──

	/**
	 * 로비에서 확정된 참가자를 등록한다. (세션 파트가 호출)
	 *
	 * 인원만이 아니라 신원까지 들고 있는 이유는, 비-심리스 ServerTravel 뒤에
	 * PlayerState 가 전부 새로 생기기 때문이다. 플레이어별 데이터를 나중에 여기 담게 되면
	 * FUniqueNetIdRepl 이 유일하게 신뢰할 수 있는 키다 — PlayerState 포인터나 접속 순서
	 * 인덱스를 키로 쓰면 레벨을 넘는 순간 전부 어긋난다.
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
	//
	// [키가 FUniqueNetIdRepl 인 이유] 비-심리스 ServerTravel 뒤에는 PlayerState 가 전부
	//   새로 생긴다. PlayerState 포인터나 접속 순서 인덱스를 키로 쓰면 저택에 도착하는 순간
	//   전부 어긋나서, 브루트를 고른 사람이 미믹으로 시작한다.

	/**
	 * 역할을 정한다. (서버 전용)
	 *
	 * @return 정해졌으면 true. 아래 두 경우에는 false 이고 **아무것도 바꾸지 않는다**
	 *           - 이 플레이어의 역할이 이미 정해져 있다 (한 번 고르면 못 바꾼다)
	 *           - 다른 플레이어가 이미 그 역할이다 (4역할은 서로 배타적이다)
	 *
	 * [왜 Set 이 아니라 Try 인가] 판정과 반영이 갈라지면 안 된다.
	 *   호출부가 "비어 있나?" 를 먼저 묻고 나중에 넣는 구조로 짜면, 두 플레이어가 같은 프레임에
	 *   같은 역할을 눌렀을 때 둘 다 통과한다. 서버에서 한 함수로 처리해야 순서가 정해진다.
	 *   UI 는 이 반환값을 보고 "이미 선택됨" 을 띄우면 된다.
	 */
	bool TrySelectRole(const FUniqueNetIdRepl& PlayerId, const FGameplayTag& RoleTag);

	/** 이 플레이어가 고른 역할. 아직 안 골랐으면 무효 태그 */
	FGameplayTag GetSelectedRole(const FUniqueNetIdRepl& PlayerId) const;

	/** 이 역할을 이미 누가 가져갔는가. UI 가 선택 목록을 흐리게 하는 데 쓴다 */
	bool IsRoleTaken(const FGameplayTag& RoleTag) const;

	// ── 진입점 선택 ──
	//
	// 기획서 2장 — 작업은 건물 외부에서 시작하고, 진입로는 장소마다 여럿이다.
	// 은신처에서 목표를 고를 때 어디로 들어갈지 함께 정하고, 작업 레벨이 그 자리에서 시작한다.
	// 밴도 같이 그리로 간다 (AHeistGameMode::PlaceVan).
	//
	// [런 단위다] BeginNewRun() 이 지운다. 다음 판은 장소가 달라서 진입점 태그도 달라진다 —
	//   저택에서 고른 Entry.Mansion.Front 를 박물관까지 들고 가면 그 레벨에 없는 태그가 되고,
	//   HeistEntryGate 가 폴백으로 떨어뜨린다. 틀리게 동작하지는 않지만 남겨 둘 이유가 없다.
	//
	// [역할과 달리 팀 단위다] 역할은 사람마다 고르지만 진입점은 팀 전체가 한 곳에서 시작한다.
	//   그래서 키가 없는 값 하나다.

	/**
	 * 진입점을 정한다. 역할과 달리 **다시 고를 수 있다** — 출발 전까지는 바꿔도 된다. (서버 전용)
	 *
	 * @return 정해졌으면 true. 무효 태그면 false 이고 **아무것도 바꾸지 않는다**
	 *
	 * [왜 여기서 레벨의 진입점 목록과 대조하지 않는가]
	 *   은신처에는 저택 레벨이 로드돼 있지 않다. 그 레벨에 그 진입점이 실제로 있는지는
	 *   도착한 뒤에야 알 수 있고, 그 판정은 HeistEntryGate 가 한다.
	 *   여기서 억지로 검사하려면 은신처가 다른 레벨의 배치를 알아야 한다.
	 */
	bool TrySelectEntry(const FGameplayTag& EntryTag);

	/** 팀이 고른 진입점(Entry.*). 아직 안 골랐으면 무효 태그 */
	UFUNCTION(BlueprintPure, Category = "Run|Entry")
	FGameplayTag GetSelectedEntry() const { return SelectedEntry; }

	/** 선택을 되돌린다. (서버 전용) 레벨의 기본 진입점에서 시작하게 된다 */
	void ClearSelectedEntry();

	// ── 구매 장비 ──
	//
	// 은신처에서 팀 공용 골드로 산다. 목록만 작업 레벨까지 건너가고, 스폰 구역에
	// 아이템 액터로 나오는 순간 소비된다. 착용·사용은 먼저 집는 사람 임자다.
	//
	// [왜 컨테이너가 아니라 수량 맵인가] 같은 장비를 여러 개 살 수 있다.
	//   FGameplayTagContainer 는 같은 태그를 두 번 담지 못해서 4인 파티가 고무창 신발을
	//   하나밖에 못 산다.

	/** 구매 목록에 추가한다. 골드 차감은 호출부가 TrySpendTeamGold 로 따로 한다. (서버 전용) */
	void AddPurchasedEquipment(const FGameplayTag& EquipmentTag, int32 Count = 1);

	/** 이 장비를 몇 개 샀는가 */
	UFUNCTION(BlueprintPure, Category = "Run|Equipment")
	int32 GetPurchasedCount(FGameplayTag EquipmentTag) const;

	/** 구매 목록 전체. 작업 레벨의 스폰 구역이 이걸 훑어 아이템을 만든다 */
	const TMap<FGameplayTag, int32>& GetPurchasedEquipment() const { return PurchasedEquipment; }

	/**
	 * 구매 목록을 비운다. 스폰이 끝난 뒤 부른다. (서버 전용)
	 *
	 * 스폰과 비우기를 갈라 두면 레벨이 두 번 로드될 때(재접속 · 리스타트) 같은 장비가
	 * 두 번 나온다. 스폰한 쪽이 바로 이어서 부를 것.
	 */
	void ConsumePurchasedEquipment();

	// ── 체포된 팀원 ──
	//
	// 도주 시간이 끝날 때 밴에 못 탄 사람은 그 판에서 잡힌다. 잡힌 사실은 판이 끝나도
	// 사라지면 안 된다 — 은신처의 '팀원 구출'(기획서)이 그것을 보고 돌아간다.
	//
	// [수명이 캠페인 단위다] BeginNewRun() 이 지우지 않는다. 구출하기 전까지 남아야
	//   하는 것이지 한 판짜리 기록이 아니다. 새 방을 열 때(ResetCampaign)만 비운다.
	//
	// [여기서 정하지 않는 것] 체포된 사람이 다음 판에 나갈 수 있는가는 이 클래스가 답하지
	//   않는다. 사실만 들고 있고, 참가 가능 여부는 그것을 읽는 쪽(로비 · 은신처)이 정한다.

	/**
	 * 이번 판에서 잡힌 사람들을 기록한다. (서버 전용 — 작업 레벨의 결과 확정 시)
	 *
	 * 이미 잡혀 있던 사람은 다시 넣지 않는다. 두 판 연속으로 잡혀도 구출은 한 번이다.
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
	 * 팀 골드를 내고 구출한다. (서버 전용)
	 *
	 * @return 잡혀 있지 않거나 잔액이 모자라면 false 이고 **아무것도 바꾸지 않는다**
	 *
	 * 비용을 인자로 받는 이유는 그것이 밸런싱이기 때문이다 — 인원수나 판 수에 따라
	 * 달라질 수 있고, 그 규칙은 은신처가 안다. 여기는 "받고 뺀다" 만 한다.
	 *
	 * 차감과 해제를 한 함수에 묶은 것은 TrySpendTeamGold 와 같은 이유다. 호출부가
	 * "잔액 되나?" 를 먼저 묻고 나중에 빼는 구조면, 두 명이 같은 프레임에 구출을 눌렀을 때
	 * 둘 다 통과해 잔액이 음수로 내려간다.
	 */
	bool TryRescue(const FUniqueNetIdRepl& PlayerId, int32 Cost);

	// ── 통과한 장소 ──
	//
	// 기획서 2장 — 저택 → 박물관 → 은행 세 곳을 모두 통과하면 최종 성공이고, 실패한 장소는
	// 처음부터 재시작한다. 그 판정의 근거가 이 목록이다.
	//
	// [수명이 캠페인 단위다] BeginNewRun() 이 지우지 않는다. 판 하나짜리 기록이 아니라
	//   "이 방에서 어디까지 왔는가" 이기 때문이다. ResetCampaign() 만 비운다.
	//   체포자 명단과 같은 이유다.
	//
	// [여기서 정하지 않는 것] 통과한 뒤에 어디로 가는가 — 은신처인지 최종 성공 연출인지는
	//   이 클래스가 답하지 않는다. 사실만 들고 있고, 이동은 그것을 읽는 쪽(세션 파트의
	//   ServerTravel)이 정한다. 최종 성공 처리가 아직 기획 미확정이라 더더욱 떠안지 않는다.

	/**
	 * 이 장소를 통과했다고 기록한다. (서버 전용 — 작업 레벨의 결과 확정 시)
	 *
	 * 같은 장소를 두 번 넣지 않는다. 재도전해서 통과해도 목록에는 한 번만 남는다.
	 *
	 * 무효 태그는 거부하고 경고를 남긴다. 사이트 태그를 지정하지 않은 GameMode 가
	 * 빈 태그를 밀어 넣으면 "몇 곳을 통과했는가" 가 조용히 틀리고, 그 값이 최종 성공을 가른다.
	 */
	void RecordSiteCleared(const FGameplayTag& SiteTag);

	/** 이 장소를 통과했는가. 목표 선택 UI 가 이미 지난 곳을 가리는 데 쓴다 */
	UFUNCTION(BlueprintPure, Category = "Run|Progress")
	bool IsSiteCleared(FGameplayTag SiteTag) const;

	/** 통과한 장소 수. 기획서의 최종 성공은 이 값이 3이 되는 것이다 */
	UFUNCTION(BlueprintPure, Category = "Run|Progress")
	int32 GetClearedSiteNum() const { return ClearedSites.Num(); }

	/** 통과한 장소들. 통과한 순서대로 쌓인다 */
	const TArray<FGameplayTag>& GetClearedSites() const { return ClearedSites; }

	// ── 수명 경계 ──

	/**
	 * 판 하나를 새로 시작한다. 런 단위 데이터(명단 · 구매 장비)를 비운다. (서버 전용)
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
	 * 플레이어별 선택 역할. 캠페인 단위 — BeginNewRun 이 지우지 않는다.
	 *
	 * UPROPERTY 가 아니다 — 키도 값도 UObject 를 참조하지 않아서 GC 가 볼 것이 없고,
	 * FUniqueNetIdRepl 을 TMap 키로 리플렉션에 올리면 UHT 쪽 제약에 걸린다.
	 */
	TMap<FUniqueNetIdRepl, FGameplayTag> SelectedRoles;

	/** 은신처에서 산 장비와 수량. 런 단위 — 작업 레벨에서 스폰되며 소비된다 */
	TMap<FGameplayTag, int32> PurchasedEquipment;

	/**
	 * 팀이 고른 진입점(Entry.*). 런 단위 — BeginNewRun 이 지운다.
	 *
	 * 무효 태그면 안 골랐다는 뜻이고, 작업 레벨이 기본 진입점으로 폴백한다.
	 */
	UPROPERTY()
	FGameplayTag SelectedEntry;

	/**
	 * 잡혀 있는 팀원. 캠페인 단위 — 구출하기 전까지 남는다.
	 *
	 * UPROPERTY 가 아닌 이유는 SelectedRoles 와 같다. FUniqueNetIdRepl 은 UObject 를
	 * 참조하지 않아 GC 가 볼 것이 없다.
	 */
	TArray<FUniqueNetIdRepl> ArrestedPlayers;

	/**
	 * 통과한 장소(Site.*). 캠페인 단위 — BeginNewRun 이 지우지 않는다.
	 *
	 * 배열인 이유는 통과 순서가 그대로 진행 경로이고, 최대 3개라 TSet 의 이점이 없기 때문이다.
	 */
	UPROPERTY()
	TArray<FGameplayTag> ClearedSites;
};
