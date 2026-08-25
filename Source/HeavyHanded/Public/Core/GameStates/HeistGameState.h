#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GameplayTagContainer.h"        // FGameplayTag 를 값으로 보유 — 전방 선언 불가
#include "Templates/SubclassOf.h"        // FHeistLoadEntry 가 값으로 보유
#include "Core/HeistPhase.h"             // EHeistPhaseReason — UPROPERTY 노출 enum 이라 전방 선언 불가
#include "Core/HeistEscapeGate.h"        // FHeistEscapeConditions — 값으로 돌려준다
#include "Core/HeistOutcome.h"           // EHeistOutcome — UPROPERTY 노출 enum 이라 전방 선언 불가
#include "HeistGameState.generated.h"

class ALootBase;
class APlayerState;

/**
 * 밴에 실린 노획물 한 건의 기록. 결과 화면의 '적재 목록' 이 이것의 배열이다.
 * 액터 포인터가 아니라 값인 것은 확정 즉시 노획물이 파괴되기 때문이다 —
 * 포인터로 들면 결과 화면이 뜰 무렵에는 전부 null 이다.
 */
USTRUCT(BlueprintType)
struct FHeistLoadEntry
{
	GENERATED_BODY()

	/** 무엇이 실렸는가. 표시용 이름 · 아이콘은 UI 가 이 클래스에서 가져온다 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Value")
	TSubclassOf<ALootBase> LootClass;

	/** 특성 태그(Loot.Type.*). 결과 화면이 중량형 · 파손형을 갈라 보여줄 때 쓴다 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Value")
	FGameplayTagContainer TypeTags;

	/** 확정 시점의 가치($). 파손 · 유출이 이미 반영된 값이다 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Value")
	int32 Value = 0;

	/** 온전했을 때의 가치($). Value 와 비교하면 얼마나 깎였는지 나온다 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Value")
	int32 BaseValue = 0;

	/** 누가 실었는가. 폰이 아니라 PlayerState 다 — 폰은 체포 · 다운으로 파괴된다 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Value")
	TObjectPtr<APlayerState> Loader = nullptr;

	/** 파손 · 유출로 가치가 깎였는가. 유도되는 사실이라 따로 저장하지 않는다 */
	bool IsValueLost() const { return Value < BaseValue; }
};

/** 접속 대기 상태 변화. 로딩 표시와 입력 차단이 붙는다. 서버 · 클라 양쪽에서 호출된다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeistStartWaitChanged, FHeistStartWaitState, State);

/** 페이즈 전환. HUD · 차단 볼륨 · 경비가 붙는다. 서버 · 클라 양쪽에서 호출된다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHeistPhaseChanged,
	FGameplayTag, NewPhase, FGameplayTag, OldPhase, EHeistPhaseReason, Reason);

/** 적재 금액 변화. HUD 목표 게이지용 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHeistLoadedValueChanged,
	int32, LoadedValue, int32, TargetValue);

/**
 * 승차 인원 변화. HUD 의 "3/4 탑승" 표시가 붙는다.
 * 생존자 수를 같이 싣는 것은 그것이 분모라서다 — HUD 가 따로 세면 "4/3 탑승" 이 뜬다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHeistBoardedChanged,
	int32, NumBoarded, int32, NumSurvivors);

/** 결과 화면 확인 인원 변화. HUD 의 "2/3 확인" 표시가 붙는다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHeistResultConfirmChanged,
	int32, NumConfirmed, int32, NumPlayers);

/**
 * 작업 레벨의 GameState. 코어 루프의 진리원이다. Prep → Heist → Escape → Result 만 다룬다.
 * 상태를 들고 복제할 뿐 스스로 바꾸지 않는다 — 무엇으로 넘어가는가는 AHeistGameMode 가 정한다.
 */
UCLASS()
class HEAVYHANDED_API AHeistGameState : public AGameState
{
	GENERATED_BODY()

	// 페이즈를 바꿀 수 있는 유일한 클래스. 상태머신 구동은 서버 전용이어야 한다
	friend class AHeistGameMode;

public:
	AHeistGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 어디서든 이걸로 접근. 작업 레벨이 아니면 nullptr 이다 */
	UFUNCTION(BlueprintPure, Category = "Heist", meta = (WorldContext = "WorldContext"))
	static AHeistGameState* Get(const UObject* WorldContext);

	// ── 페이즈 ──

	/** 현재 페이즈(Phase.*). 접속 대기 중에는 비어 있다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Phase")
	FGameplayTag GetCurrentPhase() const { return CurrentPhase; }

	/** 부모 태그 매칭이다 — Phase 하나를 넘기면 하위 페이즈가 전부 걸린다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Phase")
	bool IsPhase(FGameplayTag Phase) const { return CurrentPhase.MatchesTag(Phase); }

	/** 현재 페이즈로 넘어온 이유. 결과 화면이 "경보 발각" 과 "시간 초과" 를 구분하는 근거 */
	UFUNCTION(BlueprintPure, Category = "Heist|Phase")
	EHeistPhaseReason GetPhaseReason() const { return PhaseReason; }

	/**
	 * 이 판에 실제로 쓰인 진입점(Entry.*). 진입점이 없는 레벨이면 무효 태그.
	 * 은신처에서 고른 것과 다를 수 있다 — 그 진입점이 이 레벨에 없으면 폴백하기 때문이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Entry")
	FGameplayTag GetEntryTag() const { return EntryTag; }

	/**
	 * 현재 페이즈가 끝나기까지 남은 초. 카운트다운이 있을 때만 true 이고 OutSeconds 가 채워진다.
	 * "없음" 을 -1 이나 0 으로 표현하지 않는 것은 그 약속을 모르는 호출부가 화면에 -1초를 찍기 때문이다.
	 * 남은 초는 복제하지 않는다 — 끝나는 시각 하나만 복제하고 각자 계산한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Phase")
	bool TryGetPhaseRemainingSeconds(float& OutSeconds) const;

	UPROPERTY(BlueprintAssignable, Category = "Heist|Phase")
	FOnHeistPhaseChanged OnPhaseChanged;

	// ── 접속 대기 ──

	/** 지금 전원 접속을 기다리는 중인가. 늦게 들어온 사람도 이 값으로 현재 상태를 안다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Start")
	FHeistStartWaitState GetStartWaitState() const { return StartWaitState; }

	UPROPERTY(BlueprintAssignable, Category = "Heist|Start")
	FOnHeistStartWaitChanged OnStartWaitChanged;

	/**
	 * 접속 대기 상태를 갱신한다. **서버 전용** — AHeistGameMode 의 대기 루프만 부른다.
	 * 값이 그대로면 아무것도 하지 않는다. 0.25초마다 도는데 매번 대입하면 계속 복제된다.
	 */
	void SetStartWaitState(const FHeistStartWaitState& NewState);

	// ── 목표 금액 ──

	/** 지금까지 밴에 실은 금액($) */
	UFUNCTION(BlueprintPure, Category = "Heist|Value")
	int32 GetLoadedValue() const { return LoadedValue; }

	/** 이 장소의 목표 금액($). AHeistGameMode 가 매치 시작 시 넣는다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Value")
	int32 GetTargetValue() const { return TargetValue; }

	/** 목표 달성 여부. 초과 적재도 참이다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Value")
	bool IsTargetReached() const { return TargetValue > 0 && LoadedValue >= TargetValue; }

	/** 적재 금액을 누적한다. (서버 전용) */
	void AddLoadedValue(int32 DeltaValue);

	/**
	 * 적재 한 건을 기록하고 금액까지 누적한다. (서버 전용)
	 * 목록과 금액을 한 함수로 묶은 것은 둘이 갈라지면 결과 화면의 합계와 상단 적재액이
	 * 달라지고, 그때 어느 쪽이 맞는지 코드를 봐야만 알기 때문이다.
	 */
	void RecordLoadedLoot(const FHeistLoadEntry& Entry);

	/**
	 * 지금까지 실은 노획물 기록. 결과 화면의 '적재 목록' 이 여기서 나온다.
	 * FastArray 를 쓰지 않는다 — 항목이 30바이트 남짓이라 적재 한 번에 1KB 를 넘지 않는다.
	 */
	const TArray<FHeistLoadEntry>& GetLoadedEntries() const { return LoadedEntries; }

	UPROPERTY(BlueprintAssignable, Category = "Heist|Value")
	FOnHeistLoadedValueChanged OnLoadedValueChanged;

	// ── 탈출 ──
	//
	// 진리원은 아래 BoardedPlayers 배열이다. State.InVan 태그는 남에게 보이기 위한 미러이지
	// 판정 근거가 아니다 — 태그로는 인원을 셀 수 없고, 태그를 붙이는 GE 가 없어도 여기는 돌아야 한다.

	/** 이 플레이어가 밴에 타 있는가 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	bool IsBoarded(const APlayerState* Player) const;

	/** 밴에 타 있는 인원. 다운자도 포함된 숫자다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	int32 GetBoardedNum() const { return BoardedPlayers.Num(); }

	/**
	 * 다운되지 않고 남아 있는 인원. 탈출 판정의 분모다.
	 * 서버 · 클라 양쪽에서 유효하다 — 명단도 다운 태그도 복제된다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	int32 GetSurvivorNum() const;

	/**
	 * 지금 상황을 판정용 값으로 옮긴다.
	 * GameMode 의 종료 판정과 HUD 의 표시가 같은 함수를 거치게 하려는 것이다 —
	 * 각자 세면 "전원 탑승" 이 뜬 화면에서 판이 안 끝난다.
	 */
	FHeistEscapeConditions MakeEscapeConditions() const;

	/** 이 플레이어가 체포됐는가. 도주 시간이 끝나기 전에는 아무도 체포 상태가 아니다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	bool IsArrested(const APlayerState* Player) const;

	/** 탈출한 사람들. 결과 화면의 탈출 명단 */
	const TArray<TObjectPtr<APlayerState>>& GetBoardedPlayers() const { return BoardedPlayers; }

	/** 체포된 사람들. 결과 화면의 체포 명단 */
	const TArray<TObjectPtr<APlayerState>>& GetArrestedPlayers() const { return ArrestedPlayers; }

	/**
	 * 승차 상태를 갱신한다. (서버 전용 — AVanZone 이 부른다)
	 * State.InVan 미러도 여기서 같이 붙고 떨어진다. 따로 부르는 구조면 한쪽만 부른 경로가
	 * 반드시 생기고, 그때 명단에는 있는데 태그는 없는 사람이 나온다.
	 */
	void SetBoarded(APlayerState* Player, bool bBoarded);

	/**
	 * 체포를 확정한다. (서버 전용 — Result 진입 시 GameMode 가 한 번에 부른다)
	 * 도중에 부르지 말 것 — 아직 구조될 수 있는 사람이 체포로 찍힌다.
	 */
	void MarkArrested(APlayerState* Player);

	// ── 결과 ──
	//
	// 결과 화면이 물어보는 곳을 여기 하나로 모은다. 값의 주인이 다른 것(최다 소음 유발자)도
	// 여기서 받아 넘긴다 — UI 가 코어 루프와 경계도 컴포넌트를 각각 찾아다니지 않게.

	/**
	 * 이 판의 결과 등급. Result 진입 전에는 Failure 다.
	 * 사유(GetPhaseReason)와는 다른 값이다 — 등급은 목표 달성과 '최소 1인 탈출' 로만 정해진다.
	 * UI 가 아니라 여기서 판정하는 것은 이 값이 팀 골드 지급을 가르기 때문이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	EHeistOutcome GetOutcome() const { return Outcome; }

	/**
	 * 미션 소요 시간(초). 본 작업 진입부터 결과 확정까지 (준비 시간은 빠진다).
	 * Result 진입 순간에 서버가 한 번 고정한다 — 매번 계산하면 결과 화면에서 숫자가 계속 늘어난다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	float GetElapsedSeconds() const { return ElapsedSeconds; }

	/**
	 * 이 플레이어가 실어 온 금액 합계. 적재 목록에서 매번 계산한다.
	 * 별도 집계표를 두면 같은 사실의 진리원이 둘이 되고, 어긋났을 때 어느 쪽이 맞는지 알 수 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	int32 GetContributionOf(const APlayerState* Player) const;

	/**
	 * 결과 화면 '최다 소음 유발자'. 값의 주인은 UAlertComponent 이고 여기서는 받아 넘기기만 한다.
	 * 아무도 없거나 경계도 컴포넌트가 없으면 nullptr 이고 OutContribution 은 0 이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	APlayerState* GetNoisiestPlayer(float& OutContribution) const;

	/**
	 * 결과 화면을 확인했다고 표시한다. (서버 전용)
	 * 클라이언트 → 서버 경로는 PlayerController 의 Server RPC 다 —
	 * GameState 는 소유자가 없어 클라이언트 RPC 를 받을 수 없다.
	 */
	void SetResultConfirmed(APlayerState* Player, bool bConfirmed);

	/** 이 플레이어가 결과를 확인했는가 */
	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	bool IsResultConfirmed(const APlayerState* Player) const;

	/** 확인한 인원. HUD 의 "2/3 확인" 표시용 */
	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	int32 GetResultConfirmedNum() const { return ResultConfirmedPlayers.Num(); }

	/** 남아 있는 사람이 전부 확인했는가. **아무도 없으면 false 다** — 빈 서버가 넘어가면 안 된다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Result")
	bool AreAllResultsConfirmed() const;

	/** 확인 인원 변화. HUD 와 GameMode 의 종료 판정이 여기에 붙는다 */
	UPROPERTY(BlueprintAssignable, Category = "Heist|Result")
	FOnHeistResultConfirmChanged OnResultConfirmChanged;

	/**
	 * 이 PlayerState 를 인원 계산에 넣을 것인가. 관전자와 끊긴 뒤 남은 PlayerState 를 걸러낸다.
	 * 체포 확정도 같은 기준으로 돌아야 한다 — 모집단이 두 곳에서 갈라지면 체포되지 않은
	 * 사람이 생기면서 등급이 조용히 한 칸 올라간다. 그래서 공개돼 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	static bool IsCountedPlayer(const APlayerState* Player);

	/**
	 * 이 플레이어가 다운 상태인가. ASC 가 없으면 false.
	 * 정적인 것은 판정에 GameState 가 필요 없어서다 — PlayerState 의 ASC 만 본다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	static bool IsPlayerDowned(const APlayerState* Player);

	UPROPERTY(BlueprintAssignable, Category = "Heist|Escape")
	FOnHeistBoardedChanged OnBoardedChanged;

protected:
	/**
	 * 페이즈를 바꾼다. (서버 전용 — AHeistGameMode 만 부를 수 있다)
	 * DurationSeconds 가 0 이하면 카운트다운 없이 머문다 (Result).
	 */
	void SetPhase(const FGameplayTag& NewPhase, float DurationSeconds, EHeistPhaseReason Reason);

	/** 이 장소의 목표 금액을 설정한다. (서버 전용 — AHeistGameMode 만) */
	void SetTargetValue(int32 NewTargetValue);

	/**
	 * 이 판에 실제로 쓰인 진입점을 알린다. (서버 전용 — AHeistGameMode 만)
	 * 고른 값이 아니라 **판정을 통과한 결과**다 — 화면에 뜨는 것은 실제로 시작한 자리여야 한다.
	 */
	void SetEntryTag(const FGameplayTag& NewEntryTag);

	/**
	 * 결과 등급을 확정한다. (서버 전용 — AHeistGameMode 만)
	 * **체포를 확정한 뒤에 불러야 한다.** 탈출 여부를 체포 명단으로 보기 때문에,
	 * 순서를 뒤집으면 아무도 체포되지 않은 것으로 보여 등급이 실제보다 높게 나온다.
	 */
	void FinalizeOutcome();

	/** RepNotify 에 이전 값을 받는다 — 엔진이 넘겨주므로 따로 들고 있지 않아도 된다 */
	UFUNCTION()
	void OnRep_CurrentPhase(FGameplayTag OldPhase);

	/** 적재액과 목표액 중 하나라도 도착하면 불린다 — HUD 는 둘을 같이 그리기 때문이다 */
	UFUNCTION()
	void OnRep_LoadedValue();

	UFUNCTION()
	void OnRep_StartWaitState();

	/**
	 * 접속 대기 상태. 대기가 끝나면 bWaiting = false 로 한 번 더 복제된다 —
	 * 그 마지막 한 번이 클라이언트의 입력 차단을 푸는 신호다. 생략하면 영영 안 풀린다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_StartWaitState, BlueprintReadOnly, Category = "Heist|Start")
	FHeistStartWaitState StartWaitState;

	/**
	 * 이 판에 쓰인 진입점(Entry.*). 진입점이 없는 레벨에서는 비어 있다.
	 * OnRep 이 없는 것은 판이 시작될 때 한 번 정해지고 끝나는 값이기 때문이다.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Entry")
	FGameplayTag EntryTag;

	/** 현재 페이즈. 접속 대기 중에는 비어 있다 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, BlueprintReadOnly, Category = "Heist|Phase")
	FGameplayTag CurrentPhase;

	/**
	 * 현재 페이즈가 끝나는 서버 시각(초). 0 이면 카운트다운이 없다.
	 * TryGetPhaseRemainingSeconds() 로만 읽을 것 — 클라이언트 로컬 시계와 기준이 다르다.
	 */
	UPROPERTY(Replicated)
	float PhaseEndServerTime = 0.f;

	/**
	 * 현재 페이즈로 넘어온 이유.
	 * CurrentPhase 와 같은 RepNotify 에 묶지 못한다 — 복제가 프로퍼티 단위라 둘이 다른
	 * 프레임에 도착할 수 있다. 그래서 구독자는 델리게이트 인자로 받는다.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Phase")
	EHeistPhaseReason PhaseReason = EHeistPhaseReason::Scheduled;

	UPROPERTY(ReplicatedUsing = OnRep_LoadedValue, BlueprintReadOnly, Category = "Heist|Value")
	int32 LoadedValue = 0;

	/**
	 * 매치 시작 시 한 번 설정되고 그 뒤로 바뀌지 않는다.
	 * 그런데도 RepNotify 를 다는 것은 늦게 들어온 클라이언트 때문이다 — 그때 LoadedValue 는
	 * 아직 0 이라 알림이 안 오고, 그러면 그 사람 HUD 는 목표 게이지를 한 번도 못 그린다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_LoadedValue, BlueprintReadOnly, Category = "Heist|Value")
	int32 TargetValue = 0;

	/** 실은 노획물 기록. 적재존이 확정할 때마다 한 건씩 쌓인다 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Value")
	TArray<FHeistLoadEntry> LoadedEntries;

	/**
	 * 본 작업이 시작된 서버 시각. 소요 시간의 기준점이다.
	 * 복제하지 않는다 — 흐르는 기준점을 주면 각자 계산하다가 결과 화면에서 숫자가 계속 늘어난다.
	 */
	float HeistStartServerTime = 0.f;

	/** 미션 소요 시간(초). Result 진입 시 한 번 고정된다 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Result")
	float ElapsedSeconds = 0.f;

	/** 결과 등급. FinalizeOutcome() 이 한 번 정한다 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Result")
	EHeistOutcome Outcome = EHeistOutcome::Failure;

	/** 결과 화면을 확인한 사람들. 전원이 차면 체류 시간을 기다리지 않고 넘어간다 */
	UPROPERTY(ReplicatedUsing = OnRep_ResultConfirmedPlayers, BlueprintReadOnly, Category = "Heist|Result")
	TArray<TObjectPtr<APlayerState>> ResultConfirmedPlayers;

	UFUNCTION()
	void OnRep_ResultConfirmedPlayers();

	/**
	 * 지금 밴에 타 있는 사람들. 탈출 판정의 진리원이다.
	 * 폰이 아니라 PlayerState 를 담는다 — 폰은 체포 · 관전 전환으로 파괴되지만
	 * 판정과 결과 화면은 그 뒤까지 이어진다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_BoardedPlayers, BlueprintReadOnly, Category = "Heist|Escape")
	TArray<TObjectPtr<APlayerState>> BoardedPlayers;

	/**
	 * 체포된 사람들. Result 진입 시 한 번 채워지고 그 뒤로 바뀌지 않는다.
	 * RepNotify 가 없는 것은 의도다 — 도착 시점에는 이미 결과 화면이 페이즈 전환으로 열려 있다.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Escape")
	TArray<TObjectPtr<APlayerState>> ArrestedPlayers;

	UFUNCTION()
	void OnRep_BoardedPlayers();

private:
	/** State.InVan / State.Arrested 미러를 갱신한다. (서버 전용) */
	static void SetMirrorTag(APlayerState* Player, const FGameplayTag& Tag, bool bApply);

	/** 명단이 바뀔 때마다 서버 · 클라 양쪽에서 부른다 */
	void BroadcastBoardedChanged();
};
