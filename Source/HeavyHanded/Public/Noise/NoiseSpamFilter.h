#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * 발행 대기 1건.
 * TMap 순회 중에는 발행할 수 없어서(청취자가 반응하다 새 소음을 내면 순회 중이던 맵이
 * 재할당된다) 일단 여기에 모았다가 순회가 끝난 뒤에 내보낸다.
 */
struct FNoisePendingEmit
{
	FGameplayTag Tag;

	/** 절대 크기. 경비에게 이만큼 들린다 */
	float Loudness = 0.f;

	/**
	 * 경계도 기여 배율. 직전 발행 대비 증가분만 반영하려고 1 미만이 되며,
	 * 더 커지지 않았으면 0 이다 — 경비에겐 들리되 저택 경계도는 오르지 않는다.
	 */
	float AlertScale = 1.f;

	FVector Location = FVector::ZeroVector;
};

/** 한 프레임에 태그 4종이 동시에 터지는 일은 드물다. 그 이상은 힙으로 넘어간다 */
using FNoisePendingEmitArray = TArray<FNoisePendingEmit, TInlineAllocator<4>>;

/** 튜닝 값. UNoiseEmitterComponent 의 UPROPERTY 를 그대로 복사해 넣는다 */
struct FNoiseSpamFilterParams
{
	/** 연속 발행 체감 계수. n 번째 발행에는 이 값의 n 제곱이 곱해진다 (접수 횟수가 아니다) */
	float ConsecutiveFalloff = 0.7f;

	/** 체감 하한. 아무리 굴러도 이 배율 밑으로는 안 내려간다 */
	float ConsecutiveFloor = 0.2f;

	/** 이 시간 동안 접수가 없으면 연속 카운트를 리셋한다 */
	float ConsecutiveResetSeconds = 2.f;
};

/**
 * 태그 1종에 대한 필터 상태.
 * USTRUCT 이 아니다 — UObject 참조가 없고 순수 내부 상태라 리플렉션이 필요 없다.
 */
struct FNoiseEmitState
{
	/** 남은 쿨다운. 0 이면 즉시 발행 가능 */
	float CooldownRemaining = 0.f;

	/** 쿨다운 중에 들어온 것 중 가장 큰 소리 */
	float PendingMaxLoudness = 0.f;

	/**
	 * 직전에 발행한 절대 크기. 반드시 절대값이어야 한다.
	 * 여기에 "차액" 을 넣으면 다음 차액이 그 차액을 기준으로 계산돼 발행량이 부풀어 오른다
	 * (0.8 발행 → 0.1 발행 → 기준이 0.1 이 되어 다음에 0.75 가 나가는 식).
	 */
	float LastEmittedLoudness = 0.f;

	float TimeSinceLastHit = 0.f;

	/**
	 * 연속 "발행" 횟수. 체감 계수의 지수가 된다.
	 *
	 * 접수 횟수가 아니다 — 증가는 NotifyEmitted 가 쿨다운 주기당 1 씩 한다.
	 * 접수마다 올리면 구르는 프롭이 한 프레임에도 수십 번 부딪히면서
	 * 1초도 안 되어 ConsecutiveFloor 에 박히고, ConsecutiveFalloff 가 튜닝 값 구실을 못한다.
	 */
	int32 ConsecutiveHits = 0;

	FVector PendingLocation = FVector::ZeroVector;
};

/**
 * 물리 충돌 스팸을 "태그당 쿨다운 주기 1건" 으로 묶는 상태 머신.
 *
 * 와인 랙 같은 불안정형이 구르면 OnComponentHit 이 초당 수십 번 온다.
 * 그대로 더하면 한 번 굴린 걸로 경보 100% 가 되므로 이 필터가 소음 시스템의 핵심이다.
 *
 * 핵심 규칙 두 가지 —
 *   1. 발행은 "제 크기 그대로" 나간다. 경비에게는 실제로 그만큼 크게 들려야 한다.
 *      여기를 깎으면 전파 반경(FNoiseEvent::Radius)까지 같이 쪼그라든다.
 *   2. 억제는 AlertScale(경계도 축) 한쪽에서만 한다.
 *
 * UObject 도 UWorld 도 쓰지 않는다. 월드 없이 (입력 시퀀스 -> 발행 시퀀스) 로
 * 검증할 수 있게 하려는 것이다 — Private/Tests/NoiseSpamFilterTest.cpp 참고.
 * 이 규칙을 지키려면 액터 · 컴포넌트 · 서브시스템을 여기로 들여오지 말 것.
 */
struct FNoiseSpamFilter
{
	FNoiseSpamFilterParams Params;

	/**
	 * 소음 1건 접수. 실제 발행은 하지 않는다.
	 * 쿨다운 중에 여러 건이 들어오면 체감 계수를 먹인 뒤 가장 큰 것만 남는다.
	 */
	void Submit(FGameplayTag Tag, float Loudness, const FVector& Location);

	/**
	 * 시간을 흘리고, 쿨다운이 풀린 태그의 발행 항목을 OutEmits 에 담는다.
	 * @return 아직 살아 있는 상태가 하나라도 남았는가 (틱을 계속 돌릴지 판단용)
	 */
	bool Advance(float DeltaTime, FNoisePendingEmitArray& OutEmits);

	/** 쿨다운과 무관하게 대기 중인 것을 전부 담는다. 파괴 직전 전용 */
	void FlushAll(FNoisePendingEmitArray& OutEmits) const;

	/**
	 * 발행이 실제로 나갔음을 알린다. 쿨다운과 연속 카운트가 여기서 갱신된다.
	 * @return 갱신된 연속 발행 횟수 (로그용)
	 */
	int32 NotifyEmitted(FGameplayTag Tag, float Loudness, float CooldownSeconds);

	bool IsEmpty() const { return States.IsEmpty(); }
	void Reset() { States.Reset(); }

	/** 테스트 · 디버그용 조회 */
	const FNoiseEmitState* FindState(FGameplayTag Tag) const { return States.Find(Tag); }

private:
	TMap<FGameplayTag, FNoiseEmitState> States;
};
