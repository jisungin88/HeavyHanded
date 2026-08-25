#include "Noise/NoiseSpamFilter.h"

namespace
{
	/**
	 * 대기 중인 소리 1건을 발행 항목으로 바꾼다. 호출 전에 PendingMaxLoudness > 0 이어야 한다.
	 * Loudness 는 절대값 그대로 나가고(깎으면 전파 반경까지 쪼그라든다) 억제는 AlertScale 쪽에서만 한다.
	 * 발행 게이트가 Loudness 라 AlertScale 0 도 정상 발행된다 — 들리지만 경계도는 안 오른다.
	 */
	FNoisePendingEmit MakePendingEmit(FGameplayTag Tag, const FNoiseEmitState& State)
	{
		const float Absolute  = State.PendingMaxLoudness;
		const float Increment = FMath::Max(Absolute - State.LastEmittedLoudness, 0.f);

		return { Tag, Absolute, Increment / Absolute, State.PendingLocation };
	}
}

void FNoiseSpamFilter::Submit(FGameplayTag Tag, float Loudness, const FVector& Location)
{
	if (!Tag.IsValid() || Loudness <= 0.f)
	{
		return;
	}

	FNoiseEmitState& State = States.FindOrAdd(Tag);

	// 체감 지수는 이미 나간 발행 횟수다. 여기서 올리지 않는다 — NotifyEmitted 가 한다
	const float Falloff = FMath::Max(
			FMath::Pow(Params.ConsecutiveFalloff, static_cast<float>(State.ConsecutiveHits)),
			Params.ConsecutiveFloor);

	State.TimeSinceLastHit = 0.f;
	State.PendingLocation  = Location;

	// 가장 큰 것만 기억해 둔다. 한 프레임의 충돌이 태그별로 하나씩 묶인다
	State.PendingMaxLoudness = FMath::Max(State.PendingMaxLoudness, Loudness * Falloff);
}

bool FNoiseSpamFilter::Advance(float DeltaTime, FNoisePendingEmitArray& OutEmits)
{
	bool bAnyActive = false;

	for (TMap<FGameplayTag, FNoiseEmitState>::TIterator It(States); It; ++It)
	{
		FNoiseEmitState& State = It.Value();

		State.TimeSinceLastHit += DeltaTime;

		if (State.CooldownRemaining > 0.f)
		{
			State.CooldownRemaining = FMath::Max(State.CooldownRemaining - DeltaTime, 0.f);
		}

		// 쿨다운이 풀렸고 대기 중인 소리가 있으면 무조건 내보낸다.
		// **"직전보다 커야 한다" 는 조건을 걸면 안 된다** — 체감 때문에 두 번째부터는 항상
		// 작아지므로 첫 한 번만 소리가 나고 그 뒤로는 완전 무음이 된다
		bool bEmitQueued = false;
		if (State.CooldownRemaining <= 0.f && State.PendingMaxLoudness > 0.f)
		{
			OutEmits.Add(MakePendingEmit(It.Key(), State));
			State.PendingMaxLoudness = 0.f;
			bEmitQueued = true;
		}

		if (State.TimeSinceLastHit >= Params.ConsecutiveResetSeconds)
		{
			State.ConsecutiveHits     = 0;
			State.LastEmittedLoudness = 0.f;
		}

		// bEmitQueued 를 반드시 봐야 한다 — 첫 발행 시점에는 ConsecutiveHits 가 아직 0 이라
		// (증가는 호출부가 발행을 마친 뒤 NotifyEmitted 로 한다) 이 항목이 idle 로 판정돼 지워지고,
		// 뒤이은 NotifyEmitted 의 FindOrAdd 가 상태를 새로 만들어 연속 카운트가 영영 안 쌓인다
		const bool bIdle = !bEmitQueued
						&& State.CooldownRemaining <= 0.f
						&& State.ConsecutiveHits == 0
						&& State.PendingMaxLoudness <= 0.f;

		if (bIdle)
		{
			It.RemoveCurrent();
			continue;
		}

		bAnyActive = true;
	}

	return bAnyActive;
}

void FNoiseSpamFilter::FlushAll(FNoisePendingEmitArray& OutEmits) const
{
	// 조건은 "대기 중인 소리가 있는가" 뿐이다. 직전 발행보다 커야 한다는 조건을 걸면
	// 파손형 노획물의 마지막 한 방이 바로 그 조건에 걸려 사라진다 — 이 함수의 존재 이유가 없어진다
	for (const TPair<FGameplayTag, FNoiseEmitState>& Pair : States)
	{
		if (Pair.Value.PendingMaxLoudness > 0.f)
		{
			OutEmits.Add(MakePendingEmit(Pair.Key, Pair.Value));
		}
	}
}

int32 FNoiseSpamFilter::NotifyEmitted(FGameplayTag Tag, float Loudness, float CooldownSeconds)
{
	FNoiseEmitState& State = States.FindOrAdd(Tag);

	State.LastEmittedLoudness = Loudness;   // 절대값. 차액을 넣으면 다음 계산이 어긋난다
	State.PendingMaxLoudness  = 0.f;
	State.CooldownRemaining   = CooldownSeconds;

	// 체감 지수는 여기서 오른다 — 쿨다운 주기당 정확히 1 씩.
	// 그래야 ConsecutiveFalloff 0.7 이 "다음 발행은 70%" 라는 뜻을 실제로 갖는다
	++State.ConsecutiveHits;

	return State.ConsecutiveHits;
}
