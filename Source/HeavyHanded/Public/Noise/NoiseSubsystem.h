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
 * 감쇄 전의 소음 1건. 위치가 없는 구독자(경계도 · 기여도 집계)가 여기 붙는다.
 * 위치에 따라 다르게 들려야 하는 쪽은 INoiseListener 를 쓸 것.
 * 감쇄 전 이벤트라 BP 에 열지 않는다 — 열면 소음 판정을 BP 에서 짜게 된다.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnNoiseReported,
	const FNoiseEvent& /*Event*/, const FNoiseProfileRow& /*Profile*/, AActor* /*Instigator*/);

/**
 * 등록된 청취자 1명. OwnerActor 는 오클루전 트레이스에서 제외할 대상이다.
 * 등록 시점에 한 번만 풀어둔다 — 매번 Cast 하면 값싼 거리 컬링을 ProcessEvent 뒤로 미뤄야 한다.
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
 * 클라에서도 인스턴스는 생기지만 발행 API 가 전부 조용히 무시된다 —
 * 4명이 호출하는 API 라 게이트를 호출부가 아니라 여기 둔다.
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

	/** 지속형 소음이 없으면 아예 틱하지 않는다. 기본 TickType 이 Conditional 이라 매 프레임 반영된다 */
	virtual bool IsTickable() const override { return !ContinuousNoises.IsEmpty(); }

	// ── 소음 발행 (서버에서만 유효) ──

	/**
	 * 단발 소음. Tag 는 DT_NoiseProfiles 의 RowName 과 일치해야 한다.
	 * LoudnessScale 은 들리는 크기, AlertScale 은 경계도 기여 배율로 서로 다른 축이다 —
	 * 스팸 필터를 가진 호출부만 후자를 낮춰서 보낸다.
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

	/** 이벤트 1건을 반경 안 청취자들에게 감쇄 적용해 전달. 프로파일을 통째로 받는다 */
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