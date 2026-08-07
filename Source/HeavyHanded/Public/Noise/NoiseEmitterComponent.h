#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"          // FHitResult — UFUNCTION 시그니처라 완전한 타입이 필요하다
#include "GameplayTagContainer.h"
#include "Noise/NoiseTypes.h"          // FNoiseModifier 를 값으로 받는다
#include "NoiseEmitterComponent.generated.h"

class UPrimitiveComponent;
struct FNoiseProfileRow;

/**
 * 태그 1종에 대한 스팸 필터 상태.
 * USTRUCT 이 아니다 — UObject 참조가 없고 순수 내부 상태라 리플렉션이 필요 없다.
 */
struct FNoiseEmitState
{
	/** 남은 쿨다운. 0 이면 즉시 발행 가능 */
	float CooldownRemaining = 0.f;

	/** 쿨다운 중에 들어온 것 중 가장 큰 소리 */
	float PendingMaxLoudness = 0.f;

	/** 직전에 실제로 발행한 크기. 쿨다운 종료 시 차액만 내보내려고 들고 있는다 */
	float LastEmittedLoudness = 0.f;

	float TimeSinceLastHit = 0.f;

	/** 연속 충돌 횟수. 체감 계수의 지수가 된다 */
	int32 ConsecutiveHits = 0;

	FVector PendingLocation = FVector::ZeroVector;
};

/**
 * 물리 충돌을 소음으로 바꾼다. 노획물·장비처럼 부딪히면 소리가 나는 액터에 붙인다.
 *
 * 서버 전용 — 클라에서는 히트 델리게이트를 아예 바인딩하지 않는다.
 * 클라마다 물리 결과가 미세하게 달라서 신뢰할 수 없기 때문이다 (팀 컨벤션 3-1).
 *
 * 와인 랙 같은 불안정형이 구르면 OnComponentHit 이 초당 수십 번 온다.
 * 그대로 더하면 한 번 굴린 걸로 경보 100% 가 되므로 스팸 필터가 핵심이다.
 */
UCLASS(ClassGroup = (Noise), meta = (BlueprintSpawnableComponent))
class HEAVYHANDED_API UNoiseEmitterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNoiseEmitterComponent();

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;
	//~ End

	/**
	 * 충돌이 아닌 경로로 소음을 낸다 (던지기 · 파괴 · 떨어뜨리기 · 유출).
	 * 스팸 필터와 모디파이어를 똑같이 거친다. 서버에서만 유효하다.
	 *
	 * @param LoudnessScale  0~1. 프로파일 기본 크기에 곱해진다
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	void ReportTaggedNoise(FGameplayTag Tag, float LoudnessScale = 1.f);

	/** 장비·패시브가 등록하는 감쇄 규칙. 반환 핸들을 보관했다가 반드시 Remove 할 것 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	FGuid AddModifier(const FNoiseModifier& Modifier);

	UFUNCTION(BlueprintCallable, Category = "Noise")
	bool RemoveModifier(FGuid Handle);

protected:
	/** 물리 충돌에 쓰는 태그. DT_NoiseProfiles 의 RowName 과 일치해야 한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (Categories = "Noise"))
	FGameplayTag ImpactTag;

	/** 충돌을 감지할 프리미티브 이름. 비워두면 루트 컴포넌트를 쓴다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
	FName ImpactComponentName;

	/** 연속 충돌 체감 계수. n 번째 충돌에는 이 값의 n 제곱이 곱해진다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Spam",
			  meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ConsecutiveFalloff = 0.7f;

	/** 체감 하한. 아무리 굴러도 이 배율 밑으로는 안 내려간다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Spam",
			  meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ConsecutiveFloor = 0.2f;

	/** 이 시간 동안 충돌이 없으면 연속 카운트를 리셋한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Spam",
			  meta = (ClampMin = "0.0", Units = "s"))
	float ConsecutiveResetSeconds = 2.f;

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
				   UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool HasNoiseAuthority() const;

	/** ImpactComponentName 이 가리키는 프리미티브. 없으면 루트 */
	UPrimitiveComponent* ResolveImpactComponent() const;

	/** 모디파이어 적용 → 스팸 필터 → 발행. 모든 소음이 여기를 지난다 */
	void EmitThroughFilter(FGameplayTag Tag, float Loudness, const FVector& Location);

	/** 실제 발행 + 쿨다운 시작. TMap 순회 중에는 호출하지 말 것 */
	void EmitNow(FGameplayTag Tag, float Loudness, const FVector& Location);

	/** 프리미티브의 물리 재질 계수. 없으면 1.0 */
	float GetSurfaceCoeff(const UPrimitiveComponent* Component, const FHitResult& Hit) const;

	/** 등록된 모디파이어. FGameplayTagQuery 라 UObject 참조는 없다 */
	UPROPERTY()
	TMap<FGuid, FNoiseModifier> Modifiers;

	/** 태그별 스팸 필터 상태 */
	TMap<FGameplayTag, FNoiseEmitState> EmitStates;
};
