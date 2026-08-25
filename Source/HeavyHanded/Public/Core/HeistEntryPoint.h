#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayTagContainer.h"   // FGameplayTag 를 값으로 보유 — 전방 선언 불가
#include "HeistEntryPoint.generated.h"

class UArrowComponent;

/**
 * 작업 레벨의 진입점. 팀이 은신처에서 고르고, 도착하면 전원이 그 자리에서 시작한다. 밴도 같이 간다.
 * APlayerStart 를 상속하는 것은 엔진 스폰 경로가 그것을 전제로 돌기 때문이고,
 * 별도 클래스인 것은 "이건 진입점인가" 를 태그 문자열이 아니라 타입이 답하게 하려는 것이다.
 * 값이 전부 EditInstanceOnly 인 것은 레벨에 놓인 개체마다 달라야 하는 값이라서다.
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
	 * 밴이 서는 자리와 방향(월드 기준). 앵커가 없으면 false 이고 **밴을 옮기지 말 것.**
	 * 예전에는 진입점 자기 트랜스폼으로 폴백했는데, 밴이 스폰 지점 위로 가면서
	 * 폰 스폰이 콜리전에 막혀 아무도 폰을 갖지 못했다.
	 */
	bool TryGetVanTransform(FTransform& OutTransform) const;

	/**
	 * 이 레벨의 진입점 전부. **EntryTag 이름순으로 정렬해서** 돌려준다 —
	 * 폴백이 "첫 번째" 를 고르는데 액터 순회 순서는 보장되지 않기 때문이다.
	 * 태그가 없는 진입점은 제외하고 경고를 남긴다.
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
	 * 없어도 동작은 하지만(첫 번째로 떨어진다) 그러면 기본 진입점이 태그 이름 알파벳순으로
	 * 정해져서, 진입점을 추가하는 것만으로 기본값이 조용히 옮겨간다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Entry")
	bool bIsDefaultEntry = false;

	/**
	 * 밴이 서는 자리와 방향. **화살표가 가리키는 쪽이 밴의 정면이다.**
	 * 진입점에서 떨어뜨려 둔다 — 겹치면 스폰한 플레이어가 밴 콜리전에 낀다.
	 * 빈 컴포넌트가 아니라 화살표인 것은 안 보이면 아무도 옮기지 않기 때문이다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Entry")
	TObjectPtr<UArrowComponent> VanAnchor;
};
