#pragma once

#include "CoreMinimal.h"

/**
 * 0~1 게이지를 복제용 1바이트로 접었다 펴는 변환.
 *
 * UAlertComponent(저택 경계도)와 UPerceptionMeterComponent(경비 인지)가 같은 식을 쓴다.
 * float 를 그대로 복제하면 자연 감소 중에 매 프레임 값이 바뀌어 넷 업데이트 레이트만큼
 * 계속 전송되지만, uint8 로 끊으면 실제로 한 칸 움직였을 때만 dirty 가 된다.
 *
 * 반드시 한 곳에만 둘 것 — 서버 · 클라 · 테스트가 조금이라도 다른 식을 쓰면
 * HUD 가 서버와 어긋나고, 그 어긋남은 로그에도 컴파일 경고에도 잡히지 않는다.
 * (예전에 두 컴포넌트와 테스트가 각자 복사본을 들고 있었다. 그래서 테스트가
 *  프로덕션 코드가 아니라 자기 복사본을 검증하고 있었고, 회귀를 잡을 수 없었다)
 */
namespace Gauge01
{
	/** 되돌릴 때 오차가 반 스텝을 넘지 않게 반올림한다 */
	FORCEINLINE uint8 Quantize(float Value01)
	{
		return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Value01, 0.f, 1.f) * 255.f));
	}

	FORCEINLINE float Dequantize(uint8 Quantized)
	{
		return static_cast<float>(Quantized) / 255.f;
	}

	/** 왕복 오차 상한. 테스트가 이 값을 그대로 쓴다 */
	inline constexpr float MaxRoundTripError = 0.5f / 255.f;
}
