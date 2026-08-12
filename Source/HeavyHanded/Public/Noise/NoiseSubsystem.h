#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"          // FHitResult — 스크래치 버퍼를 값으로 들고 있어 완전한 타입이 필요하다
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Noise/NoiseTypes.h"
#include "NoiseSubsystem.generated.h"

class AActor;
class INoiseListener;
class UDataTable;

/**
 * 감쇄가 적용되기 전의 소음 1건. 저택 전체가 공유하는 정보라 청취자 위치 개념이 없다.
 *
 * 경계도(UAlertComponent)와 "최다 소음 유발자" 집계처럼 위치가 없는 구독자가 여기에 붙는다.
 * 경비처럼 위치에 따라 다르게 들려야 하는 쪽은 INoiseListener 를 쓸 것.
 *
 * BP 노출이 필요 없는 C++ 전용 훅이라 다이내믹 델리게이트로 두지 않았다.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnNoiseReported,
	const FNoiseEvent& /*Event*/, const FNoiseProfileRow& /*Profile*/, AActor* /*Instigator*/);

/**
 * 등록된 청취자 1명.
 *
 * OwnerActor 는 오클루전 트레이스에서 제외할 대상이다 (자기 콜리전에 막히면 안 된다).
 * 등록 시점에 한 번만 풀어둔다 — 예전에는 소음 1건 × 청취자마다 Cast 를 두 번씩 다시 했고,
 * 값싼 거리 컬링을 GetListenerLocation(ProcessEvent) 뒤로 미룰 수밖에 없었다.
 *
 * 컴포넌트의 소유 액터는 런타임에 바뀌지 않으므로 등록 때 확정해도 안전하다.
 */
struct FNoiseListenerEntry
{
	TWeakObjectPtr<UObject> Listener;
	TWeakObjectPtr<AActor>  OwnerActor;
};

/** 지속형 소음 1건 (대형 금고 절단 등). 핸들로 시작/정지한다 */
USTRUCT()
struct FContinuousNoise
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	TWeakObjectPtr<AActor> Source;

	/** 다음 발행까지 남은 시간 */
	float TimeUntilNextEmit = 0.f;
};

/**
 * 소음의 발행 · 전파 · 감쇄. 서버 권위 100%.
 * 클라에서도 인스턴스는 생성되지만 발행 API가 전부 조용히 무시된다.
 * (호출부가 IsServer 체크를 안 해도 되게 하려는 것. 4명이 호출하는 API라 실수 여지를 없앤다)
 */
UCLASS()
class HEAVYHANDED_API UNoiseSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 어디서든 이걸로 접근. 월드가 없으면 nullptr */
	static UNoiseSubsystem* Get(const UObject* WorldContext);

	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * 지속형 소음이 하나도 없으면 아예 틱하지 않는다.
	 * 기본 TickType 은 Conditional 이라 이 값이 매 프레임 반영된다
	 * (IsAllowedToTick 은 UTickableWorldSubsystem 에서 final 이라 건드릴 수 없다).
	 */
	virtual bool IsTickable() const override { return !ContinuousNoises.IsEmpty(); }

	// ── 소음 발행 (서버에서만 유효) ──

	/**
	 * 단발 소음. Tag는 DT_NoiseProfiles의 RowName과 일치해야 한다.
	 *
	 * @param LoudnessScale  프로파일 기본값에 곱하는 배율. 결과는 0~1로 클램프된다
	 * @param AlertScale     경계도 기여 배율. 기본 1.0 — 스팸 필터를 가진 호출부만 낮춰서 보낸다.
	 *                       들리는 크기(LoudnessScale)에는 영향을 주지 않는다
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	void ReportNoise(FGameplayTag Tag, FVector Location, float LoudnessScale = 1.f, AActor* Instigator = nullptr,
					 float AlertScale = 1.f);

	/** 지속 소음 시작. 반환 핸들을 보관했다가 반드시 Stop 할 것 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	FGuid StartContinuousNoise(FGameplayTag Tag, AActor* Source);

	UFUNCTION(BlueprintCallable, Category = "Noise")
	void StopContinuousNoise(FGuid Handle);

	// ── 청취자 ──

	void RegisterListener(TScriptInterface<INoiseListener> Listener);
	void UnregisterListener(TScriptInterface<INoiseListener> Listener);

	/** 태그로 프로파일 조회. 정확히 일치하는 행이 없으면 부모 태그로 거슬러 올라간다 */
	const FNoiseProfileRow* FindProfile(FGameplayTag Tag);

	// ── 구독 ──

	/** 감쇄 전 이벤트. Propagate 직전에 서버에서만 브로드캐스트된다 */
	FOnNoiseReported OnNoiseReported;

private:
	// 권위 판정은 Shared/NetAuthority.h 의 자유 함수 하나로 통일했다.
	// 예전에는 여기와 컴포넌트 셋이 각자 다른 기준을 들고 있었다

	/**
	 * 이벤트 1건을 반경 안 청취자들에게 감쇄 적용해 전달.
	 * 프로파일을 통째로 받는다 — 필드를 풀어서 넘기면 뭘 더 쓸 때마다 시그니처가 늘어나고,
	 * 바로 위에서 브로드캐스트하는 OnNoiseReported 와 모양도 어긋난다
	 */
	void Propagate(const FNoiseEvent& Event, const FNoiseProfileRow& Profile);

	/** 거리 + 오클루전 감쇄. 0이면 안 들림 */
	float ComputeAttenuation(const FVector& From, const FVector& To, float Radius,
							 const AActor* IgnoreA, const AActor* IgnoreB) const;

	UDataTable* GetProfileTable();

	/** 소프트 참조를 한 번만 로드해서 붙들어 둔다 */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CachedProfileTable = nullptr;

	bool bProfileTableResolved = false;

	/** 이미 경고한 미싱 태그. 물리 충돌 경로라 태그당 한 번만 찍어야 한다 */
	TSet<FGameplayTag> WarnedMissingProfiles;

	/** 죽거나 파괴된 청취자가 알아서 빠지도록 약참조 */
	TArray<FNoiseListenerEntry> Listeners;

	/**
	 * 오클루전 트레이스 결과 재사용 버퍼. 트레이스마다 힙 할당이 나지 않게 하려는 것이다.
	 * ComputeAttenuation 이 const 라 mutable 이다. 게임 스레드 전용.
	 */
	mutable TArray<FHitResult> OcclusionHitsScratch;

	TMap<FGuid, FContinuousNoise> ContinuousNoises;
	
	/** GameState 가 준비되면 경계도 컴포넌트를 붙인다. 서버 전용 */
	void HandleGameStateSet(AGameStateBase* GameState);

	FDelegateHandle GameStateSetHandle;
};