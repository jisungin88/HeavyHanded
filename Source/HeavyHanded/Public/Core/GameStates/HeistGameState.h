#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GameplayTagContainer.h"        // FGameplayTag 를 값으로 보유 — 전방 선언 불가
#include "Templates/SubclassOf.h"        // FHeistLoadEntry 가 값으로 보유
#include "Core/HeistPhase.h"             // EHeistPhaseReason — UPROPERTY 노출 enum 이라 전방 선언 불가
#include "Core/HeistEscapeGate.h"        // FHeistEscapeConditions — 값으로 돌려준다
#include "HeistGameState.generated.h"

class ALootBase;
class APlayerState;

/**
 * 밴에 실린 노획물 한 건의 기록. 결과 화면의 '적재 목록'(기획서 8장)이 이것의 배열이다.
 *
 * [왜 액터 포인터가 아니라 값인가]
 *   적재가 확정되면 노획물 액터는 사라진다 (이펙트 후 파괴). 포인터로 들고 있으면
 *   결과 화면이 뜰 무렵에는 전부 null 이라 무엇을 실었는지 아무것도 남지 않는다.
 *   확정 순간의 사실만 복사해 두면 액터의 수명과 무관해진다.
 *
 * [적재자가 폰이 아니라 PlayerState 인 이유]
 *   폰은 체포 · 다운 · 관전 전환으로 파괴된다. 결과 화면까지 살아남는 것은 PlayerState 뿐이고,
 *   ASC 를 PlayerState 에 둔 것과 같은 이유다.
 *
 * [이름 · 아이콘이 없는 이유]
 *   ALootBase 에 표시용 이름 필드가 없다. 클래스만 들고 있으면 UI 가 거기서 이름과 아이콘을
 *   해결할 수 있고, 나중에 노획물 파트가 DisplayName 을 추가해도 이 구조체는 그대로다.
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

	/** 누가 실었는가. 기여도 집계의 키. 굴러 들어온 물건은 비어 있다 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Value")
	TObjectPtr<APlayerState> Loader = nullptr;

	/**
	 * 파손 · 유출로 가치가 깎였는가.
	 *
	 * 별도 bool 로 저장하지 않는다 — 두 값에서 유도되는 사실을 따로 들고 있으면
	 * 둘이 어긋날 수 있고, 어긋난 쪽이 맞는지 아무도 모른다.
	 */
	bool IsValueLost() const { return Value < BaseValue; }
};

/**
 * 페이즈 전환. HUD · 레벨 차단 볼륨 · 경비 증원이 여기에 붙는다.
 * 서버와 클라이언트 양쪽에서 호출된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHeistPhaseChanged,
	FGameplayTag, NewPhase, FGameplayTag, OldPhase, EHeistPhaseReason, Reason);

/** 적재 금액 변화. HUD 목표 게이지용 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHeistLoadedValueChanged,
	int32, LoadedValue, int32, TargetValue);

/**
 * 승차 인원 변화. HUD 의 "3/4 탑승" 표시가 여기에 붙는다.
 *
 * 생존자 수를 같이 싣는 이유는 그것이 분모이기 때문이다. 승차 인원만 보내면 HUD 가
 * 분모를 따로 세야 하고, 세는 시점이 달라 화면에 "4/3 탑승" 이 뜰 수 있다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHeistBoardedChanged,
	int32, NumBoarded, int32, NumSurvivors);

/**
 * 작업(저택 · 박물관 · 은행) 레벨의 GameState. 코어 루프의 진리원이다.
 *
 * [범위] Prep → Heist → Escape → Result 만 다룬다.
 *   Phase.Lobby / Phase.Hideout 은 여기 오지 않는다 — 그 둘은 레벨 자체가 다르고
 *   전환은 ServerTravel 이다 (세션 파트). 은신처는 AShelterGameState 가 따로 있다.
 *
 * [역할 분담] 이 클래스는 상태를 들고 복제할 뿐, 스스로 바꾸지 않는다.
 *   "언제 무엇으로 넘어가는가" 는 AHeistGameMode 가 정하고, 전이 규칙 자체는
 *   HeistPhase 네임스페이스에 있다. 상태 변경 함수를 friend 로 좁혀 둔 이유가 이것이다 —
 *   아무나 페이즈를 바꿀 수 있으면 전이 규칙이 여러 곳으로 새어 나간다.
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
	 * 현재 페이즈가 끝나기까지 남은 초.
	 *
	 * @param OutSeconds  카운트다운이 있을 때만 채워진다
	 * @return            카운트다운이 있으면 true. Result 와 접속 대기는 false
	 *
	 * [왜 float 하나로 안 돌려주는가] "카운트다운 없음" 을 -1 이나 0 으로 표현하면
	 * 호출부가 그 약속을 알아야 하고, 모르면 화면에 -1초가 그대로 찍힌다.
	 * 반환 타입이 그 구분을 강제하게 둔다.
	 *
	 * [왜 남은 시간을 복제하지 않는가] 남은 초를 복제하면 매 프레임 값이 달라져
	 * GameState 의 넷 업데이트 레이트만큼 미션 내내 계속 전송된다.
	 * 대신 '끝나는 시각' 하나만 복제하고 남은 시간은 각자 계산한다 — 페이즈당 1회면 끝난다.
	 * 기준 시계인 AGameStateBase::GetServerWorldTimeSeconds() 는 엔진이 이미 복제하고 있어
	 * 우리가 추가로 내는 비용이 없고, 클라이언트 프레임레이트와 무관하게 정확하다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Phase")
	bool TryGetPhaseRemainingSeconds(float& OutSeconds) const;

	UPROPERTY(BlueprintAssignable, Category = "Heist|Phase")
	FOnHeistPhaseChanged OnPhaseChanged;

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

	/**
	 * 적재 금액을 누적한다. (서버 전용)
	 *
	 * 페이즈와 달리 friend 로 좁히지 않는다. 부르는 쪽이 밴 적재존이고, 앞으로도
	 * "노획물이 실렸다" 를 아는 액터가 여럿 생긴다. 규칙이 아니라 누적일 뿐이라 위험하지 않다.
	 */
	void AddLoadedValue(int32 DeltaValue);

	/**
	 * 적재 한 건을 기록하고 금액까지 누적한다. (서버 전용)
	 *
	 * 목록과 금액을 한 함수로 묶은 이유는 둘이 갈라지면 안 되기 때문이다. 호출부가
	 * 따로 부르는 구조면 어느 한쪽만 부른 경로가 생기고, 그때 결과 화면의 항목 합계와
	 * 상단의 적재액이 다르게 찍힌다 — 어느 쪽이 맞는지 코드를 봐야만 알 수 있다.
	 */
	void RecordLoadedLoot(const FHeistLoadEntry& Entry);

	/**
	 * 지금까지 실은 노획물 기록. 결과 화면의 '적재 목록'이 여기서 나온다.
	 *
	 * [지금은 서버에만 있다] 복제하지 않는다. 결과 화면을 붙이는 Day 4 에 복제 수단을
	 *   정한다 — 그때까지 클라이언트에서는 항상 비어 있으므로 UI 를 여기 붙이지 말 것.
	 */
	const TArray<FHeistLoadEntry>& GetLoadedEntries() const { return LoadedEntries; }

	UPROPERTY(BlueprintAssignable, Category = "Heist|Value")
	FOnHeistLoadedValueChanged OnLoadedValueChanged;

	// ── 탈출 ──
	//
	// [진리원은 명단이다] 누가 탔는가는 아래 BoardedPlayers 배열 하나가 정한다.
	//   State.InVan 태그도 같이 붙지만 그것은 남에게 보이기 위한 미러이지 판정 근거가 아니다.
	//   태그를 진리원으로 삼으면 인원을 셀 때마다 전원을 순회해야 하고, 세는 시점에 따라
	//   답이 달라진다. 그리고 태그를 붙이는 시스템(전영배 GE)이 아직 없어도 여기는 돌아야 한다.

	/** 이 플레이어가 밴에 타 있는가 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	bool IsBoarded(const APlayerState* Player) const;

	/** 밴에 타 있는 인원. 다운자도 포함된 숫자다 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	int32 GetBoardedNum() const { return BoardedPlayers.Num(); }

	/**
	 * 다운되지 않고 남아 있는 인원. 탈출 판정의 분모다.
	 *
	 * 서버 · 클라이언트 양쪽에서 유효하다 — 명단은 복제되고, 다운 여부는 ASC 태그라
	 * 그것도 복제된다. HUD 가 서버에 물어보지 않아도 같은 숫자를 얻는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	int32 GetSurvivorNum() const;

	/**
	 * 지금 상황을 판정용 값으로 옮긴다.
	 *
	 * GameMode 의 종료 판정과 HUD 의 표시가 같은 함수를 거치게 하는 것이 목적이다.
	 * 둘이 각자 세면 "전원 탑승" 이 뜬 화면에서 판이 안 끝나는 상황이 생긴다.
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
	 *
	 * State.InVan 태그도 여기서 같이 붙고 떨어진다. 명단과 태그를 한 함수에 묶어 둔 이유는
	 * 둘이 갈라지면 안 되기 때문이다 — 따로 부르는 구조면 한쪽만 부른 경로가 반드시 생기고,
	 * 그때 명단에는 있는데 태그는 없는 사람이 나와서 어느 쪽이 맞는지 코드를 봐야 알게 된다.
	 */
	void SetBoarded(APlayerState* Player, bool bBoarded);

	/**
	 * 체포를 확정한다. (서버 전용 — AHeistGameMode 가 Result 진입 시 한 번에 부른다)
	 *
	 * 도중에 부르지 말 것. 체포는 도주 시간이 끝나는 순간에만 성립하는 상태이고,
	 * 그 전에 붙이면 아직 구조될 수 있는 사람이 체포로 찍힌다.
	 */
	void MarkArrested(APlayerState* Player);

	/**
	 * 이 플레이어가 다운 상태인가. ASC 가 없으면 false.
	 *
	 * 정적 함수인 이유는 판정에 GameState 가 필요 없기 때문이다 — PlayerState 의 ASC 만 본다.
	 * 결과 집계처럼 GameState 밖에서도 같은 기준으로 물어봐야 하는 곳이 있어 공개한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Heist|Escape")
	static bool IsPlayerDowned(const APlayerState* Player);

	UPROPERTY(BlueprintAssignable, Category = "Heist|Escape")
	FOnHeistBoardedChanged OnBoardedChanged;

protected:
	/**
	 * 페이즈를 바꾼다. (서버 전용 — AHeistGameMode 만 부를 수 있다)
	 *
	 * @param NewPhase          Phase.* 태그
	 * @param DurationSeconds   0 이하면 카운트다운 없이 머문다 (Result)
	 * @param Reason            왜 넘어왔는가. 결과 화면까지 실려 간다
	 */
	void SetPhase(const FGameplayTag& NewPhase, float DurationSeconds, EHeistPhaseReason Reason);

	/** 이 장소의 목표 금액을 설정한다. (서버 전용 — AHeistGameMode 만) */
	void SetTargetValue(int32 NewTargetValue);

	/**
	 * RepNotify 에 이전 값을 받는다.
	 *
	 * UAlertComponent 는 이전 단계를 따로 들고 있어야 했지만(LastBroadcastLevel),
	 * 그건 복제와 무관한 서버 측 방송까지 같은 함수로 처리했기 때문이다.
	 * 페이즈는 파라미터 있는 RepNotify 로 엔진에서 이전 값을 받아 온다.
	 */
	UFUNCTION()
	void OnRep_CurrentPhase(FGameplayTag OldPhase);

	/** 적재액과 목표액 중 하나라도 도착하면 불린다 — HUD 는 둘을 같이 그리기 때문이다 */
	UFUNCTION()
	void OnRep_LoadedValue();

	/** 현재 페이즈. 접속 대기 중에는 비어 있다 */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase, BlueprintReadOnly, Category = "Heist|Phase")
	FGameplayTag CurrentPhase;

	/**
	 * 현재 페이즈가 끝나는 서버 시각(초). 0 이면 카운트다운이 없다.
	 * TryGetPhaseRemainingSeconds() 로만 읽을 것 — 이 값 자체는 클라이언트 로컬 시계와 기준이 다르다.
	 */
	UPROPERTY(Replicated)
	float PhaseEndServerTime = 0.f;

	/**
	 * 현재 페이즈로 넘어온 이유.
	 *
	 * CurrentPhase 와 짝이라 같은 RepNotify 를 걸고 싶지만, 복제는 프로퍼티 단위라
	 * 둘이 다른 프레임에 도착할 수 있다. 순서를 보장할 수 없으므로 이유를 먼저 갱신하고
	 * 페이즈를 나중에 갱신하는 식의 가정을 하지 않는다 — 구독자는 델리게이트 인자로 받는다.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Phase")
	EHeistPhaseReason PhaseReason = EHeistPhaseReason::Scheduled;

	UPROPERTY(ReplicatedUsing = OnRep_LoadedValue, BlueprintReadOnly, Category = "Heist|Value")
	int32 LoadedValue = 0;

	/**
	 * 매치 시작 시 한 번 설정되고 그 뒤로 바뀌지 않는다.
	 *
	 * 그런데도 RepNotify 를 다는 이유는 늦게 들어온 클라이언트 때문이다. 그때 LoadedValue 는
	 * 아직 0(기본값과 같음)이라 RepNotify 가 불리지 않는데, 목표액은 0 이 아니라서 불린다.
	 * 이것이 없으면 그 클라이언트의 HUD 는 목표 게이지를 한 번도 못 그린다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_LoadedValue, BlueprintReadOnly, Category = "Heist|Value")
	int32 TargetValue = 0;

	/**
	 * 실은 노획물 기록. 적재존이 확정할 때마다 한 건씩 쌓인다.
	 *
	 * UPROPERTY 인 이유는 항목이 APlayerState 를 참조하기 때문이다 — GC 가 보게 해야 한다.
	 * 복제 지정자가 없는 것은 의도다. 위 GetLoadedEntries() 주석 참고.
	 */
	UPROPERTY()
	TArray<FHeistLoadEntry> LoadedEntries;

	/**
	 * 지금 밴에 타 있는 사람들. 탈출 판정의 진리원이다.
	 *
	 * 배열인 이유는 순서(누가 먼저 탔는가)가 결과 화면에 쓰일 수 있고, 인원이 최대 4명이라
	 * TSet 의 이점이 없기 때문이다. 복제되는 값이 작을수록 좋다.
	 *
	 * 폰이 아니라 PlayerState 를 담는다 — 폰은 체포 · 관전 전환으로 파괴되지만
	 * 판정과 결과 화면은 그 뒤까지 이어진다.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_BoardedPlayers, BlueprintReadOnly, Category = "Heist|Escape")
	TArray<TObjectPtr<APlayerState>> BoardedPlayers;

	/**
	 * 체포된 사람들. Result 진입 시 한 번 채워지고 그 뒤로 바뀌지 않는다.
	 *
	 * RepNotify 가 없는 것은 의도다 — 이 값이 도착하는 시점에는 이미 Phase.Result 로
	 * 넘어가 있고, 결과 화면은 페이즈 전환으로 열린다. 명단은 그때 읽으면 된다.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Heist|Escape")
	TArray<TObjectPtr<APlayerState>> ArrestedPlayers;

	UFUNCTION()
	void OnRep_BoardedPlayers();

private:
	/**
	 * 이 PlayerState 를 인원 계산에 넣을 것인가.
	 *
	 * 관전자와 접속이 끊긴 뒤 남아 있는 PlayerState 를 걸러낸다. 이걸 안 걸면
	 * 나간 사람이 영영 안 탄 것으로 남아서 "생존자 전원 승차" 가 성립하지 않고,
	 * 남은 사람들이 도주 시간을 끝까지 기다려야 한다.
	 */
	static bool IsCountedPlayer(const APlayerState* Player);

	/** State.InVan / State.Arrested 미러를 갱신한다. (서버 전용) */
	static void SetMirrorTag(APlayerState* Player, const FGameplayTag& Tag, bool bApply);

	/** 명단이 바뀔 때마다 서버 · 클라 양쪽에서 부른다 */
	void BroadcastBoardedChanged();
};
