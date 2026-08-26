#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class APawn;

/**
 * 문 열기, 상점 열기, 상자 열기 등 '집어서 드는 것'이 아닌 즉발성 상호작용 전용 인터페이스.
 * 노획물(ICarryable)은 포함하지 않는다 — 판단 기준(GetPrimaryCarrier 등)이 완전히 달라서
 * 같이 묶으면 GAB_Interact 쪽 분기만 더 헷갈리게 된다.
 *
 * BlueprintNativeEvent 로 열어서 C++/블루프린트 양쪽에서 구현 가능하게 한다 — 문/상점/상자
 * 담당 팀원이 블루프린트로 작업할 수도 있기 때문이다.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class HEAVYHANDED_API IInteractable
{
	GENERATED_BODY()

public:
	/**
	 * 상호작용을 실행한다. (서버 전용 — GAB_Interact 는 서버 권한에서만 이 함수를 호출한다)
	 * 성공/실패 판정과 그에 따른 피드백은 구현체(담당 팀원)가 알아서 책임진다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnInteract(APawn* Interactor);
};
