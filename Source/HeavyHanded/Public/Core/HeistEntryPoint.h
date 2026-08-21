#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayTagContainer.h"   // FGameplayTag 를 값으로 보유 — 전방 선언 불가
#include "HeistEntryPoint.generated.h"

class UArrowComponent;

/**
 * 작업 레벨의 진입점. 기획서 2장 — "본 작업은 건물 외부에서 시작한다."
 *
 * 팀은 은신처에서 이 중 하나를 고르고, 저택에 도착하면 **전원이 그 자리에서 시작한다.**
 * 밴도 같이 그리로 간다 — 진입점은 곧 밴에서 내리는 자리다.
 *
 * [왜 APlayerStart 를 상속하는가]
 *   엔진의 스폰 경로(AGameModeBase::ChoosePlayerStart)가 APlayerStart 를 전제로 돈다.
 *   따로 만든 액터로 두면 그 경로를 통째로 우회해야 하고, PIE 의 "플레이어 시작 위치" 표시나
 *   네비게이션 검증 같은 에디터 편의도 전부 잃는다.
 *
 * [왜 기본 APlayerStart + PlayerStartTag 로 하지 않았는가]
 *   진입점에는 표시 이름과 설명이 붙는다 — 은신처 선택 UI 가 "정문 · 시야 노출 높음" 을
 *   그려야 한다. PlayerStartTag 는 FName 하나뿐이라 UI 가 문자열을 해석하게 되고,
 *   그 해석 규칙이 곧 또 하나의 암묵적 계약이 된다.
 *
 *   그리고 레벨에는 진입점이 아닌 PlayerStart 도 생긴다(테스트용 · 관전 시작 위치).
 *   클래스로 갈라 두면 "이건 진입점인가" 를 태그 문자열이 아니라 타입이 답한다.
 *
 * [값은 전부 EditInstanceOnly 다]
 *   진입점은 **레벨에 놓인 개체마다 달라야 하는** 값이다. 아키타입 기본값으로 두면
 *   BP 서브클래스를 진입점 수만큼 만들게 된다 (문서 05 — 노출 수준).
 */
UCLASS(Blueprintable)
class HEAVYHANDED_API AHeistEntryPoint : public APlayerStart
{
	GENERATED_BODY()

public:
	AHeistEntryPoint(const FObjectInitializer& ObjectInitializer);

	/** 이 진입점의 식별자(Entry.*). 은신처의 선택이 레벨을 건너올 때 쓰는 키다 */
	UFUNCTION(BlueprintPure, Category = "Entry")
	FGameplayTag GetEntryTag() const { return EntryTag; }

	/** 선택 UI 에 뜰 이름. 비어 있으면 UI 가 EntryTag 를 그대로 보여주면 된다 */
	UFUNCTION(BlueprintPure, Category = "Entry")
	FText GetDisplayName() const { return DisplayName; }

	/** 선택 UI 의 한 줄 설명 — "시야 노출 높음, 도주로 많음" */
	UFUNCTION(BlueprintPure, Category = "Entry")
	FText GetDescription() const { return Description; }

	/** 아무것도 고르지 않았을 때 여기서 시작하는가 */
	UFUNCTION(BlueprintPure, Category = "Entry")
	bool IsDefaultEntry() const { return bIsDefaultEntry; }

	/**
	 * 밴이 서는 자리와 방향(월드 기준). 화살표가 가리키는 쪽이 밴의 뒷문이다.
	 *
	 * 진입점 액터 자체가 아니라 자식 앵커를 쓰는 이유는 **둘이 같은 자리면 안 되기 때문**이다 —
	 * 플레이어가 밴 콜리전 안에서 스폰된다. 레벨 디자이너가 에디터에서 화살표만 끌어 옮기면 된다.
	 *
	 * @return 앵커가 없으면 false 이고 OutTransform 은 건드리지 않는다. **밴을 옮기지 말 것.**
	 *
	 * [왜 실패를 값으로 표현하지 않는가]
	 *   예전에는 앵커가 없으면 진입점 자기 트랜스폼을 돌려줬다. 그 결과 밴이 **플레이어
	 *   스폰 지점 위로** 이동했고, 폰 스폰이 콜리전에 막혀 아무도 폰을 갖지 못했다.
	 *   밴 배치가 실패했을 뿐인데 판 전체가 멈춘 것이다.
	 *
	 *   "모르겠으면 스폰 지점" 은 어떤 상황에서도 정답이 아니다. 반환 타입이 그 구분을
	 *   강제하게 둔다 — AHeistGameState::TryGetPhaseRemainingSeconds 와 같은 판단이다.
	 */
	bool TryGetVanTransform(FTransform& OutTransform) const;

	/**
	 * 이 레벨의 진입점 전부. **EntryTag 이름순으로 정렬해서** 돌려준다.
	 *
	 * 정렬이 계약인 이유는 폴백이 "첫 번째" 를 고르기 때문이다 (HeistEntryGate).
	 * 액터 순회 순서는 보장되지 않아서, 정렬하지 않으면 같은 레벨인데도 실행할 때마다
	 * 다른 곳에서 시작한다. 태그가 없는 진입점은 제외하고 경고를 남긴다.
	 */
	static void CollectEntryPoints(const UObject* WorldContext, TArray<AHeistEntryPoint*>& OutEntries);

	/** 태그로 찾는다. 없으면 nullptr. 정확히 일치하는 것만 본다 (부모 매칭 아님) */
	static AHeistEntryPoint* FindByTag(const UObject* WorldContext, const FGameplayTag& Tag);

	/**
	 * Entries 에서 기본 진입점의 인덱스. 지정된 것이 없으면 INDEX_NONE.
	 *
	 * 여럿이 체크돼 있으면 **앞선 것**(태그 이름순)을 쓰고 경고한다. 하나를 고르는 것이
	 * 이 플래그의 뜻이므로, 둘 다 존중하는 해석이 없다.
	 */
	static int32 FindDefaultIndex(const TArray<AHeistEntryPoint*>& Entries);

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	/** 태그를 비워 두거나 겹치게 둔 것을 저장 시점에 알린다 */
	virtual void CheckForErrors() override;
#endif

	/**
	 * 이 진입점의 식별자(Entry.*).
	 *
	 * 비워 두면 이 진입점은 목록에서 빠진다 — 고를 수 없는 진입점이 되므로 반드시 지정할 것.
	 * 한 레벨에 같은 태그가 둘 있으면 먼저 찾은 쪽이 이긴다 (BeginPlay 가 경고한다).
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Entry")
	FGameplayTag EntryTag;

	/** 선택 UI 에 뜰 이름 — "정문", "지하 주차장", "뒷골목" */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Entry")
	FText DisplayName;

	/** 선택 UI 의 한 줄 설명 — 무엇을 감수하는 진입인가 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Entry", meta = (MultiLine = "true"))
	FText Description;

	/**
	 * 아무것도 고르지 않았을 때 여기서 시작한다. **레벨마다 하나만 체크할 것.**
	 *
	 * 은신처를 거치지 않고 레벨을 바로 열거나(PIE), 고른 진입점이 이 레벨에서 사라졌을 때
	 * 떨어지는 자리다. 기획서의 "정문 앞" 처럼 가장 무난한 곳에 체크한다.
	 *
	 * [왜 필요한가] 이 플래그가 없으면 기본 진입점이 **태그 이름 알파벳순**으로 정해진다.
	 *   Entry.Mansion.Alley 를 추가하는 것만으로 기본값이 정문에서 뒷골목으로 조용히 옮겨간다 —
	 *   아무도 그런 의도로 태그를 짓지 않는다.
	 *
	 * 아무 데도 체크하지 않아도 동작은 한다(첫 번째로 떨어진다). 다만 그 '첫 번째' 가
	 * 위와 같은 이유로 흔들리므로 하나는 체크해 두는 것이 좋다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Entry")
	bool bIsDefaultEntry = false;

	/**
	 * 밴이 서는 자리와 방향. **화살표가 가리키는 쪽이 밴의 정면이다.**
	 *
	 * 진입점에서 조금 떨어뜨려 둔다 — 겹친 상태면 스폰한 플레이어가 밴 콜리전에 낀다.
	 * 기본값(뒤로 400cm)은 안전한 추측일 뿐이고, 실제 위치는 레벨마다 다르다.
	 * 에디터에서 이 화살표를 끌어 진입로 · 램프 · 골목에 맞춰 놓는다.
	 *
	 * [왜 빈 SceneComponent 가 아니라 화살표인가]
	 *   빈 컴포넌트는 뷰포트에 아무것도 그리지 않는다 — 옮겨야 하는 것이 보이지 않으면
	 *   아무도 옮기지 않고, 밴이 벽 안에 서 있는 것을 플레이해 보고서야 알게 된다.
	 *   방향도 정해야 하는 값이라 화살표가 맞다. 게임 중에는 숨겨진다(bHiddenInGame).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Entry")
	TObjectPtr<UArrowComponent> VanAnchor;
};
