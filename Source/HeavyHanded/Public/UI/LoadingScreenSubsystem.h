#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"   // FGameplayTag — UFUNCTION 파라미터라 전방 선언이 불가능하다
#include "UI/LoadingTypes.h"        // FLoadingScreenContent — UFUNCTION 반환이라 전방 선언이 불가능하다
#include "LoadingScreenSubsystem.generated.h"

class UDataTable;
class ULoadingScreenWidget;

/**
 * 레벨 이동 로딩 화면 (기획서 8장 UI).
 *
 * [왜 GameInstance 인가] 논심리스 트래블은 월드를 통째로 버린다. 월드 · 게임스테이트 ·
 *   플레이어 컨트롤러에 붙은 것은 로딩이 시작되는 순간 같이 죽는다.
 *   **트래블을 넘어 살아남는 것은 GameInstance 하나뿐**이라 여기여야 한다.
 *   기존 로딩 위젯이 "클라에서 안 뜨던" 것도 같은 이유다 — 띄우자마자 월드와 함께 사라졌다.
 *
 * [왜 MoviePlayer 인가] 로딩 중에는 게임 스레드가 LoadMap 안에서 멈춘다. 뷰포트에 올린
 *   UMG 위젯은 그 사이 한 프레임도 그려지지 않는다. 엔진이 별도 스레드로 그려 주는
 *   창구가 MoviePlayer 뿐이고, 그래서 Build.cs 에 이 모듈이 들어가 있다.
 *
 * [PIE 에서는 뜨지 않는다] 무비 플레이어는 에디터 플레이에 없다(IsMoviePlayerEnabled 이 false).
 *   화면을 눈으로 맞출 때는 hh.UI.Loading 치트로 뷰포트에 직접 띄우고,
 *   진짜 동작은 Standalone 이나 패키징에서 확인한다.
 *
 * [접속 대기(2부)와 같은 문구를 쓴다] MakeContent() 를 열어 둔 이유다.
 *   로딩이 끝난 뒤 뜨는 접속 대기 화면이 제목과 팁을 다시 만들지 않고 이걸 부른다
 */
UCLASS()
class HEAVYHANDED_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 다음 트래블의 목적지를 알려 둔다. **출발을 부르기 직전에** 부른다.
	 *
	 * 로딩이 시작된 뒤에는 월드가 없어 아무것도 조회할 수 없다.
	 * 알려 주지 않으면 제목이 "작업 장소 진입 중…" 같은 일반 문구로 나간다 — 뜨긴 뜬다
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	void SetNextDestination(FGameplayTag SiteTag);

	/** 지금(또는 방금) 향하는 장소. 접속 대기 화면이 제목을 이어 쓰려고 읽는다 */
	UFUNCTION(BlueprintPure, Category = "UI|Loading")
	FGameplayTag GetDestinationSite() const { return DestinationSite; }

	/**
	 * 화면에 꽂을 문구 한 벌을 만든다.
	 *
	 * StatusOverride 가 비어 있으면 "레벨 로딩 중…"(설정값)이 들어간다.
	 * 팁은 부를 때마다 새로 뽑히므로, 같은 화면을 유지하려는 쪽은 결과를 들고 있어야 한다
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	FLoadingScreenContent MakeContent(FGameplayTag SiteTag, const FText& StatusOverride);

	/**
	 * 설정에 지정된 로딩 위젯을 만든다. 뷰포트에 올리지는 않는다.
	 *
	 * 접속 대기 화면도 이걸로 만들어 같은 모양을 쓴다. 클래스가 비었으면 null 이다
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|Loading")
	ULoadingScreenWidget* CreateScreenWidget();

private:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** DT_LoadingTips 를 한 번만 로드한다. 실패도 "해봤다"로 기억해 매번 다시 시도하지 않는다 */
	void EnsureTipsLoaded();

	/** SiteTag 에 맞는 팁 하나. 표가 없거나 맞는 행이 없으면 빈 FText 다 */
	FText PickTip(FGameplayTag SiteTag);

	/** 다음 트래블의 목적지. 트래블을 넘어 남아야 해서 멤버다 */
	UPROPERTY()
	FGameplayTag DestinationSite;

	/**
	 * 지금 무비 플레이어에 넘어가 있는 위젯.
	 *
	 * 로딩이 끝나도 비우지 않는다 — 무비 플레이어가 아직 이 위젯의 Slate 쪽을 들고 있을 수
	 * 있어서, 여기서 놓으면 GC 가 먼저 회수해 버린다. 다음 트래블 때 교체한다
	 */
	UPROPERTY()
	TObjectPtr<ULoadingScreenWidget> ActiveWidget;

	/** 로드해 둔 팁 표 */
	UPROPERTY()
	TObjectPtr<UDataTable> TipsTable;

	/** 표를 한 번이라도 로드해 봤는가 */
	bool bTipsResolved = false;

	/** 직전에 뽑은 팁. 두 번 연속 같은 팁이 나오는 것만 막는다 */
	FString LastTipKey;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
};
