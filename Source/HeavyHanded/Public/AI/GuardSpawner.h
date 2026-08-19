#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AI/GuardTypes.h"         // EPatrolPattern — DrawNextPatrolPattern() 의 기본 멤버 초기화에 실제 값이 필요하다
#include "GuardSpawner.generated.h"

class AGuardCharacter;
class USceneComponent;

// 레벨에 배치해 경비를 여러 명 스폰한다. 스폰된 경비는 전부 이 스포너가 들고 있는
// PatrolPoints 를 공유하고, PatrolPattern(Loop/PingPong/Random)은 경비마다 무작위로
// 정해진다 - 같은 순찰 목록을 도는 경비끼리 진행 순서가 겹치지 않게 하기 위함이다.
// "가방 뽑기" 방식(DrawNextPatrolPattern)이라 이 스포너가 연속으로 스폰하는 경비끼리는
// 절대 같은 패턴을 받지 않는다.
//
// GuardAIController::SelectInitialPatrolIndex 의 "이웃 순번 기반 시작 지점 분산"과는
// 별개다 - 그건 시작 지점을, 이건 순찰 순서 자체를 흩뜨린다.
//
// UAlertComponent 의 병력 증원(OnReinforcementTriggered - 추격이 일정 횟수 쌓이면 발동)을
// 구독해, 발동될 때마다 이 스포너에도 경비 1명을 추가로 스폰한다.
UCLASS()
class HEAVYHANDED_API AGuardSpawner : public AActor
{
	GENERATED_BODY()

public:
	AGuardSpawner();

	// 스폰할 경비 블루프린트/클래스. AutoPossessAI 가 PlacedInWorldOrSpawned 로
	// 잡혀 있어야 스폰과 동시에 AGuardAIController 가 빙의한다 (BP_GuardBase 계열 확인할 것).
	UPROPERTY(EditAnywhere, Category = "Guard|Spawner")
	TSubclassOf<AGuardCharacter> GuardClass;

	// 스폰할 경비 수.
	UPROPERTY(EditAnywhere, Category = "Guard|Spawner", meta = (ClampMin = "0"))
	int32 GuardCount = 1;

	// 경비 1명을 스폰한 뒤 다음 1명을 스폰하기까지의 간격. 0이면 한꺼번에 다 스폰한다.
	UPROPERTY(EditAnywhere, Category = "Guard|Spawner", meta = (ClampMin = "0.0", Units = "s"))
	float SpawnInterval = 5.f;

	// 스폰된 경비 전원이 공유할 순찰 지점. 레벨에 배치한 액터(타겟 포인트 등)를 끌어다 놓는다.
	UPROPERTY(EditInstanceOnly, Category = "Guard|Spawner")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	// true면 BeginPlay에서 자동으로 스폰한다.
	UPROPERTY(EditAnywhere, Category = "Guard|Spawner")
	bool bSpawnOnBeginPlay = true;

	// GuardCount 만큼 경비를 SpawnInterval 간격으로 스폰한다. 1명은 즉시, 나머지는
	// 타이머로 하나씩 뒤이어 스폰된다. 서버 권위 폰이라 서버에서만 실행된다.
	UFUNCTION(BlueprintCallable, Category = "Guard|Spawner")
	void SpawnGuards();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Guard|Spawner")
	TObjectPtr<USceneComponent> SpawnRoot;

private:
	// 경비 1명을 스폰하고 PatrolPoints/PatrolPattern/빙의까지 적용한다.
	void SpawnSingleGuard();

	// SpawnInterval 타이머가 부른다. 다음 1명을 스폰하고, 목표 수에 도달하면 스스로 멈춘다.
	void HandleSpawnTimer();

	// UAlertComponent::OnReinforcementTriggered 구독 콜백. 경비 1명을 즉시 추가 스폰한다.
	UFUNCTION()
	void HandleReinforcement(int32 ReinforcementIndex);

	// 다음 경비에게 줄 순찰 패턴을 뽑는다. "가방 뽑기" 방식 - Loop/PingPong/Random 3개를
	// 섞어 하나씩 소진하고, 다 뽑히면 다시 섞어 채운다. 그래서 이 스포너가 연속으로
	// 스폰하는 경비끼리는(가방을 다 비우기 전까지) 같은 패턴이 절대 겹치지 않는다.
	// 새로 채운 가방의 첫 패턴이 직전에 준 것과 같은 "가방 경계" 케이스도 따로 바꿔치기해서
	// 막는다 - 그래야 몇 명을 스폰하든 바로 이웃한 두 경비의 패턴이 항상 다르다.
	EPatrolPattern DrawNextPatrolPattern();

	// 지금까지 스폰한 수. SpawnGuards() 를 다시 호출해도 깨끗이 이어가도록 시작할 때 0으로 되돌린다.
	int32 SpawnedCount = 0;

	FTimerHandle SpawnTimerHandle;

	// DrawNextPatrolPattern() 의 상태. 스포너 수명 내내 유지되며(재스폰·병력 증원 포함),
	// UPROPERTY 로 노출할 이유가 없어 순수 C++ 멤버로 둔다.
	TArray<EPatrolPattern> PatternBag;
	EPatrolPattern LastDrawnPattern = EPatrolPattern::Loop;
	bool bHasDrawnPattern = false;
};
