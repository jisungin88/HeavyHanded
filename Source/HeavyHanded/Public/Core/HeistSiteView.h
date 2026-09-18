#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"       // FGameplayTag 를 값으로 보유 — 전방 선언 불가
#include "UObject/SoftObjectPtr.h"      // TSoftObjectPtr 를 값으로 보유
#include "HeistSiteView.generated.h"

class UTexture2D;

/**
 * 한 장소를 화면에 그리는 데 필요한 것 전부. 은신처의 목표 표시 · 출발 문 위젯이 쓴다.
 *
 * **복제하지 않는다** — 값이 전부 DT_SiteCatalog 에서 나와 모든 머신에서 같고,
 * 유일한 변수인 '어느 장소인가' 는 FRunProgressView::NextSite 가 이미 복제한다.
 * 그래서 클라이언트가 자기 자리에서 계산한다.
 *
 * 표의 행(FHeistSiteRow)을 그대로 넘기지 않는 이유가 셋이다 —
 *   ① UI 가 Level 경로를 알 필요가 없다
 *   ② bCleared 는 이번 판의 진행 상황이라 설정에 없다
 *   ③ bReady 를 위젯마다 다시 조합하면 출발 문과 목표 표시가 다른 답을 낸다
 *
 * FRunProgressView 와 같은 역할이다.
 */
USTRUCT(BlueprintType)
struct FHeistSiteView
{
	GENERATED_BODY()

	/** 장소 식별자(Site.*). 무효면 갈 곳이 없다 — 전 장소 통과 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	FGameplayTag SiteTag;

	/** 화면에 뜰 이름 — "박물관". 표에 없으면 UUISettings 의 폴백 문구가 들어온다 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	FText DisplayName;

	/** 한 줄 설명. **비어 있을 수 있다** — 그때는 위젯이 그 줄을 그리지 않는다 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	FText Description;

	/** 목표 선택 · 출발 문에 띄울 그림. 비어 있으면 위젯이 이미지 칸을 숨긴다 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	TSoftObjectPtr<UTexture2D> Image;

	/** 목표 금액($) */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	int32 TargetValue = 0;

	/** 본 작업 제한 시간(초). 준비 45초는 포함되지 않는다 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	float HeistSeconds = 0.f;

	/** 도주 시간(초). 경보 100% 또는 시간 만료로 들어간다 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	float EscapeSeconds = 0.f;

	/** 고를 수 있는 진입점 수. 0 이면 선택 UI 를 숨긴다 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	int32 EntryNum = 0;

	/** 이미 통과한 장소인가 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	bool bCleared = false;

	/**
	 * 출발할 수 있는가. 레벨과 목표 금액이 둘 다 있어야 참이다.
	 *
	 * 거짓이면 출발 문이 버튼을 잠그고 이유를 띄운다. 지금은 IngameTravel 이 내부에서
	 * 조용히 실패하고 로그만 남겨서, 플레이어는 "E 를 눌렀는데 아무 일도 안 일어난다" 로 겪는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Site")
	bool bReady = false;
};
