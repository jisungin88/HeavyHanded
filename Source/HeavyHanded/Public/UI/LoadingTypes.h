#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"      // FTableRowBase — 상속이라 전방 선언이 불가능하다
#include "GameplayTagContainer.h"  // FGameplayTag — USTRUCT 멤버라 전방 선언이 불가능하다
#include "LoadingTypes.generated.h"

/**
 * 로딩 화면에 뜨는 팁 한 줄 (DT_LoadingTips 의 행).
 *
 * 행 이름은 아무거나 좋다 — 팁은 태그로 찾는 것이 아니라 SiteTag 로 걸러 무작위로 뽑는다.
 * (DT_NoiseProfiles 처럼 RowName 을 태그와 맞출 필요가 없다는 뜻이다)
 */
USTRUCT(BlueprintType)
struct FLoadingTipRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 화면에 뜨는 문구. 시안 기준 두 줄까지다 — 세 줄이 넘으면 TIP 블록이 제목을 밀어낸다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading|Tip", meta = (MultiLine = "true"))
	FText TipText;

	/**
	 * 이 팁이 붙는 장소. 비워 두면 어느 장소에서나 나온다.
	 *
	 * 장소별 팁이 하나도 없어도 공용 팁이 뽑히도록 하기 위한 것이다 —
	 * 표를 다 채우기 전에도 화면이 비지 않는다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading|Tip", meta = (Categories = "Site"))
	FGameplayTag SiteTag;
};

/**
 * 로딩 화면에 꽂을 문구 한 벌.
 *
 * 위젯이 스스로 만들지 않고 통째로 받는다 — 로딩 중에는 게임 스레드가 멈춰 있어서
 * 위젯이 무언가를 조회할 수 없다. 넘기는 순간 이미 완성돼 있어야 한다
 */
USTRUCT(BlueprintType)
struct FLoadingScreenContent
{
	GENERATED_BODY()

	/** 제목 위 작은 글씨 — "다음 작업" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
	FText Eyebrow;

	/** 제목 — "박물관 진입 중…" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
	FText Title;

	/** 팁 머리말 — "TIP" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
	FText TipLabel;

	/** 팁 본문 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (MultiLine = "true"))
	FText Tip;

	/**
	 * 하단 상태 문구 — "레벨 로딩 중…" / "동료를 기다리는 중 2/4".
	 *
	 * 이 자리만 로딩(1부)과 접속 대기(2부)에서 다르다. 나머지는 그대로 이어져
	 * 플레이어 눈에는 한 화면이 계속 떠 있는 것으로 보인다
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
	FText Status;
};
