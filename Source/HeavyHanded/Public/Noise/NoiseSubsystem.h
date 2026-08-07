#pragma once

#include "CoreMinimal.h"
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

	// ── 소음 발행 (서버에서만 유효) ──

	/**
	 * 단발 소음. Tag는 DT_NoiseProfiles의 RowName과 일치해야 한다.
	 * @param LoudnessScale  프로파일 기본값에 곱하는 배율. 결과는 0~1로 클램프된다
	 */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	void ReportNoise(FGameplayTag Tag, FVector Location, float LoudnessScale = 1.f, AActor* Instigator = nullptr);

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
	bool HasNoiseAuthority() const;

	/** 이벤트 1건을 반경 안 청취자들에게 감쇄 적용해 전달 */
	void Propagate(const FNoiseEvent& Event, ENoiseGrade Grade, bool bGlobal);

	/** 거리 + 오클루전 감쇄. 0이면 안 들림 */
	float ComputeAttenuation(const FVector& From, const FVector& To, float Radius,
							 const AActor* IgnoreA, const AActor* IgnoreB) const;

	UDataTable* GetProfileTable();

	/** 소프트 참조를 한 번만 로드해서 붙들어 둔다 */
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CachedProfileTable = nullptr;

	bool bProfileTableResolved = false;

	/** 죽거나 파괴된 청취자가 알아서 빠지도록 약참조 */
	TArray<TWeakObjectPtr<UObject>> Listeners;

	TMap<FGuid, FContinuousNoise> ContinuousNoises;
};