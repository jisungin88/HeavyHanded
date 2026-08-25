#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HeistPhase.generated.h"

/**
 * 접속 대기 상태. 페이즈가 시작되기 **전** 구간이다 (CurrentPhase 는 아직 비어 있다).
 * 값 넷을 한 구조체로 묶는 것은 RepNotify 를 한 번만 받기 위해서다 —
 * 따로 두면 인원만 바뀌었을 때(3/4 → 4/4) 알림이 안 와서 화면 숫자가 멈춘다.
 */
USTRUCT(BlueprintType)
struct FHeistStartWaitState
{
	GENERATED_BODY()

	/** 아직 기다리는 중인가. false 면 아래 값들은 의미가 없다 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Start")
	bool bWaiting = false;

	/** 지금까지 들어온 인원 (로딩 중과 관전자 포함) */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Start")
	int32 NumConnected = 0;

	/** 이 판에 올 인원. **0 이면 모른다는 뜻이다** — 화면에 "3/0" 을 띄우지 말 것 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Start")
	int32 NumExpected = 0;

	/** 접속은 했으나 아직 레벨 로딩이 끝나지 않은 인원 */
	UPROPERTY(BlueprintReadOnly, Category = "Heist|Start")
	int32 NumTravelling = 0;
};

/**
 * 페이즈가 바뀐 이유. Escape 에 들어간 뒤로는 복원할 방법이 없어 전이와 함께 실어 나른다 —
 * 결과 화면이 "경보 발각" 과 "시간 초과" 를 구분해야 한다.
 */
UENUM(BlueprintType)
enum class EHeistPhaseReason : uint8
{
	/** 앞 페이즈의 시간이 다 됐다 */
	Scheduled   UMETA(DisplayName = "Scheduled"),

	/** 경보 100% — 경계도가 래치됐다 */
	Alarm       UMETA(DisplayName = "Alarm"),

	/** 생존 인원이 전부 밴에 탔다 */
	AllEscaped  UMETA(DisplayName = "All Escaped"),

	/** 치트로 넘겼다. 결과 집계에서 제외할 판단 근거가 된다 */
	Cheat       UMETA(DisplayName = "Cheat")
};

/**
 * 작업 레벨의 페이즈 순서와 전이 규칙. GameMode 밖에 있는 것은 서버 객체 없이도
 * "Heist 다음이 뭐지" 를 물을 수 있어야 하기 때문이다 (HUD · 자동화 테스트).
 * **전이는 GetOrder() 배열 하나에서 전부 나온다** — 표를 둘로 나누면 어긋나도 안 잡힌다.
 */
namespace HeistPhase
{
	/** 진행 순서. Prep → Heist → Escape → Result */
	HEAVYHANDED_API const TArray<FGameplayTag>& GetOrder();

	/** 순서상 몇 번째인가. 페이즈 태그가 아니면 INDEX_NONE */
	HEAVYHANDED_API int32 IndexOf(const FGameplayTag& Phase);

	/** 다음 페이즈. 마지막(Result)이거나 순서에 없는 태그면 무효 태그를 돌려준다 */
	HEAVYHANDED_API FGameplayTag GetNext(const FGameplayTag& Phase);

	/** 로그용. 열거형 값을 그대로 찍으면 숫자만 나와서 읽을 수 없다 */
	HEAVYHANDED_API const TCHAR* ToString(EHeistPhaseReason Reason);
}
