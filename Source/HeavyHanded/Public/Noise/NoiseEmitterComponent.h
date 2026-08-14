#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"          // FHitResult — UFUNCTION 시그니처라 완전한 타입이 필요하다
#include "GameplayTagContainer.h"
#include "Noise/NoiseSpamFilter.h"     // FNoiseSpamFilter 를 값으로 들고 있다
#include "Noise/NoiseTypes.h"          // FNoiseModifier 를 값으로 받는다
#include "NoiseEmitterComponent.generated.h"

class UCurveFloat;
class UPhysicalMaterial;
class UPrimitiveComponent;
struct FNoiseProfileRow;

/**
 * 물리 충돌을 소음으로 바꾼다. 노획물·장비처럼 부딪히면 소리가 나는 액터에 붙인다.
 *
 * 서버 전용 — 클라에서는 히트 델리게이트를 아예 바인딩하지 않는다.
 * 클라마다 물리 결과가 미세하게 달라서 신뢰할 수 없기 때문이다 (문서 02 네트워크).
 *
 * 충돌 스팸을 묶는 상태 머신은 FNoiseSpamFilter 에 따로 있다. 이 컴포넌트가 하는 일은
 * "물리 히트 -> 크기" 변환과 모디파이어 적용, 그리고 필터가 뱉은 것을 서브시스템에 넘기는 것뿐이다.
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

	/** 연속 발행 체감 계수. n 번째 발행에는 이 값의 n 제곱이 곱해진다 (충돌 횟수가 아니다) */
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

	/** ImpactComponentName 이 가리키는 프리미티브. 없으면 루트. BeginPlay 에서 한 번만 부른다 */
	UPrimitiveComponent* ResolveImpactComponent() const;

	/** ImpactTag 의 프로파일 값을 캐시하고 커브를 미리 로드한다 */
	void CacheImpactProfile();

	/** UPROPERTY 튜닝 값을 필터에 복사한다. 디자이너가 PIE 중에 만질 수 있어서 매 틱 새로 넣는다 */
	FNoiseSpamFilterParams MakeFilterParams() const;

	/** 대기 중인 소음을 쿨다운과 무관하게 지금 전부 내보낸다. 파괴 직전에만 쓴다 */
	void FlushPendingEmits();

	/** 모디파이어 적용 → 스팸 필터 접수. 모든 소음이 여기를 지난다 */
	void EmitThroughFilter(FGameplayTag Tag, float Loudness, const FVector& Location);

	/**
	 * 필터가 뱉은 항목 1건을 실제로 발행하고 쿨다운을 시작한다.
	 *
	 * @param bKeepTicking  false 면 틱을 다시 켜지 않는다. 파괴 도중(FlushPendingEmits)에는
	 *                      쿨다운을 내려줄 다음 틱 자체가 없으므로 켜봐야 의미가 없다
	 */
	void EmitNow(const FNoisePendingEmit& Emit, bool bKeepTicking);

	/**
	 * 프리미티브의 물리 재질 계수. 없으면 1.0.
	 *
	 * @param PreferredMaterial  "맞은 지점" 의 재질. 있으면 바디 기본값보다 우선한다.
	 *                           HitComponent 쪽에는 넘기지 말 것 — HandleHit 주석 참고
	 */
	float GetSurfaceCoeff(const UPrimitiveComponent* Component, const UPhysicalMaterial* PreferredMaterial) const;

	/** 등록된 모디파이어. FGameplayTagQuery 라 UObject 참조는 없다 */
	UPROPERTY()
	TMap<FGuid, FNoiseModifier> Modifiers;

	/** 충돌 스팸을 태그별로 묶는 상태 머신. 로직은 전부 저쪽에 있다 */
	FNoiseSpamFilter SpamFilter;

	/** 캐시된 충돌 프로파일. 캐시가 없으면 충돌 소음을 내지 않는다 */
	TWeakObjectPtr<UPrimitiveComponent> ImpactComponent;

	// ── ImpactTag 프로파일 캐시 ──
	//
	// 물리 히트는 구르는 프롭 하나가 초당 수십 번씩 발생시킨다.
	// 그때마다 DataTable 을 다시 뒤질 이유가 없다 — ImpactTag 는 런타임에 안 바뀐다.
	//
	// 대신 게임 도중 DT_NoiseProfiles 를 다시 임포트해도 이 값들은 갱신되지 않는다.
	// 임펄스 밸런싱을 만졌으면 PIE 를 다시 시작할 것.

	bool  bImpactProfileCached = false;
	float CachedMinImpulse     = 0.f;
	float CachedMaxImpulse     = 1000.f;
	float CachedCooldownSeconds = 0.25f;

	/** 미리 해석해 둔 커브. 첫 충돌 때 물리 콜백 안에서 동기 로드가 걸리지 않게 한다 */
	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> CachedImpactCurve = nullptr;
};
