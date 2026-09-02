#include "Alert/AlertComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#include "Alert/AlertSettings.h"
#include "Shared/Gauge01.h"
#include "Shared/NetAuthority.h"
#include "Noise/NoiseSubsystem.h"
#include "Noise/NoiseTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlert, Log, All);

namespace
{
	/**
	 * 임계값 비교 여유. float 오차 한두 ULP 를 흡수한다.
	 * UE 가 /fp:fast 로 빌드해서 x / 100.f 가 x * 0.01f 로 바뀌면 67 이 0.669999957 이 되고,
	 * "67% 인데 경계 단계가 아닌" 상태가 된다. 히스테리시스 간격보다 400배 작다.
	 */
	constexpr float AlertThresholdTolerance = 1.e-4f;

	/** Value 가 Threshold 이상인가. 경계에서의 float 오차를 흡수한다 */
	FORCEINLINE bool AtLeast(float Value, float Threshold)
	{
		return Value >= Threshold - AlertThresholdTolerance;
	}

	/** 소음을 낸 액터에서 책임질 플레이어를 역추적한다 */
	APlayerState* ResolvePlayerState(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState();
		}

		// 던져진 노획물처럼 폰이 아닌 경우 — 액터의 Instigator 컨트롤러를 따라간다
		if (const AController* Controller = Actor->GetInstigatorController())
		{
			return Controller->PlayerState;
		}

		return nullptr;
	}
}

// ──────────────────────────────────────────────────────────────
// 수명 주기
// ──────────────────────────────────────────────────────────────

UAlertComponent::UAlertComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // 게이지가 0 이면 돌 필요 없다

	// 이 틱이 하는 일은 무소음 시간 누적과 자연 감소뿐이다.
	// 기본 감소율이 1%/초라 프레임마다 볼 이유가 없고, 감소가 선형이라
	// 간격을 벌려도 결과가 수학적으로 동일하다 (DeltaTime 이 그만큼 커진다).
	// 144fps 기준 틱 횟수가 14분의 1이 된다
	PrimaryComponentTick.TickInterval = 0.1f;

	SetIsReplicatedByDefault(true);
}

void UAlertComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAlertComponent, ReplicatedGauge);
	DOREPLIFETIME(UAlertComponent, AlertLevel);

	// 결과 화면은 클라에서 뜨는데 NoiseContribution 맵은 서버에만 있다.
	// 1위만 복제해서 GetNoisiestPlayer() 가 양쪽에서 같은 답을 주게 한다
	DOREPLIFETIME(UAlertComponent, NoisiestPlayer);
	DOREPLIFETIME(UAlertComponent, NoisiestContribution);
}

void UAlertComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasServerAuthority(this))
	{
		return;   // 클라는 복제된 값만 받는다
	}

	if (UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this))
	{
		NoiseReportedHandle =
				Subsystem->OnNoiseReported.AddUObject(this, &UAlertComponent::HandleNoiseReported);
	}
	else
	{
		UE_LOG(LogAlert, Warning, TEXT("NoiseSubsystem 을 찾지 못해 경계도가 소음에 반응하지 않습니다."));
	}
}

void UAlertComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NoiseReportedHandle.IsValid())
	{
		if (UNoiseSubsystem* Subsystem = UNoiseSubsystem::Get(this))
		{
			Subsystem->OnNoiseReported.Remove(NoiseReportedHandle);
		}
		NoiseReportedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

// ──────────────────────────────────────────────────────────────
// 접근 · 부착
// ──────────────────────────────────────────────────────────────

UAlertComponent* UAlertComponent::Get(const UObject* WorldContext)
{
	if (!GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->FindComponentByClass<UAlertComponent>() : nullptr;
}

UAlertComponent* UAlertComponent::EnsureOnGameState(AGameStateBase* GameState)
{
	if (!IsValid(GameState) || !GameState->HasAuthority())
	{
		return nullptr;   // 클라에는 복제로 도착한다. 직접 만들면 안 된다
	}

	if (UAlertComponent* Existing = GameState->FindComponentByClass<UAlertComponent>())
	{
		return Existing;
	}

	UAlertComponent* Component = NewObject<UAlertComponent>(GameState, TEXT("AlertComponent"));
	Component->RegisterComponent();

	UE_LOG(LogAlert, Log, TEXT("경계도 컴포넌트를 %s 에 부착했습니다."), *GameState->GetName());
	return Component;
}

// ──────────────────────────────────────────────────────────────
// 소음 수신
// ──────────────────────────────────────────────────────────────

void UAlertComponent::HandleNoiseReported(const FNoiseEvent& Event, const FNoiseProfileRow& Profile, AActor* Instigator)
{
	if (!HasServerAuthority(this) || AlertLevel == EAlertLevel::Alarm)
	{
		return;   // 경보는 래치라 더 올릴 것도 없다
	}

	// 소리 크기에 비례해 오른다. 7단계 공식: AlertDelta = Row.AlertDelta * Loudness01
	//
	// AlertScale 은 발행측 스팸 필터가 넘겨주는 "새로운 정보량" 이다.
	// 구르는 와인 랙은 경비에게 매번 제대로 들려야 하므로 Loudness01 을 깎으면 안 되고,
	// 대신 이쪽만 낮춰서 한 번 굴린 것으로 경보가 차지 않게 한다
	const float Delta = Profile.AlertDelta * Event.Loudness01 * Event.AlertScale;

	SilenceTimer = 0.f;   // 소음이 나면 무소음 타이머 리셋

	if (Delta <= 0.f)
	{
		return;
	}

	// 결과 화면 "최다 소음 유발자" 집계 (기획서 8장).
	// 여기서 안 모으면 나중에 소음 이벤트를 다시 파고들어야 한다
	if (APlayerState* PlayerState = ResolvePlayerState(Instigator))
	{
		const float Total = (NoiseContribution.FindOrAdd(PlayerState) += Delta);

		// 기여량은 단조 증가라 1위 갱신이 O(1) 이다 — 맵을 다시 훑을 필요가 없다.
		// 같은 사람이 계속 1위면 값만 오르고, 순위가 뒤집힐 때만 대상이 바뀐다
		if (Total > NoisiestContribution)
		{
			NoisiestPlayer       = PlayerState;
			NoisiestContribution = Total;
		}
	}

	ApplyGauge(AlertGauge + Delta);
	SetComponentTickEnabled(true);
}

// ──────────────────────────────────────────────────────────────
// 게이지 · 단계
// ──────────────────────────────────────────────────────────────

EAlertLevel UAlertComponent::EvaluateLevel(float Gauge, EAlertLevel Current) const
{
	if (Current == EAlertLevel::Alarm)
	{
		return EAlertLevel::Alarm;   // 래치. ResetAlert() 만이 푼다
	}

	if (AtLeast(Gauge, 1.f))
	{
		return EAlertLevel::Alarm;
	}

	// Get() 은 CDO 라 null 이 아니다 (AlertSettings.h 주석 참고)
	const UAlertSettings& Settings = *UAlertSettings::Get();

	// 진입은 Enter, 이탈은 Exit 임계값을 쓴다. 이 간격(히스테리시스)이 없으면
	// 33/34% 경계에서 단계가 깜빡이면서 셔터가 열렸다 닫혔다 한다.
	//
	// 한 번에 두 단계를 건너뛰는 경우도 처리한다 — 평온에서 특대 소음 한 방에
	// 67% 를 넘으면 의심을 거치지 않고 바로 경계로 가야 한다
	switch (Current)
	{
	case EAlertLevel::Calm:
		if (AtLeast(Gauge, Settings.AlertedEnter))    { return EAlertLevel::Alerted; }
		return AtLeast(Gauge, Settings.SuspiciousEnter) ? EAlertLevel::Suspicious : EAlertLevel::Calm;

	case EAlertLevel::Suspicious:
		if (AtLeast(Gauge, Settings.AlertedEnter))    { return EAlertLevel::Alerted; }
		return AtLeast(Gauge, Settings.SuspiciousExit) ? EAlertLevel::Suspicious : EAlertLevel::Calm;

	case EAlertLevel::Alerted:
		if (!AtLeast(Gauge, Settings.SuspiciousExit)) { return EAlertLevel::Calm; }
		return AtLeast(Gauge, Settings.AlertedExit) ? EAlertLevel::Alerted : EAlertLevel::Suspicious;

	default:
		return Current;
	}
}

void UAlertComponent::ApplyGauge(float NewGauge)
{
	const float Clamped = FMath::Clamp(NewGauge, 0.f, 1.f);
	const EAlertLevel NextLevel = EvaluateLevel(Clamped, AlertLevel);

	// **정확히 비교한다.** IsNearlyEqual(1e-4) 을 쓰면 100fps 이상에서 프레임당 감소량이
	// 그보다 작아 통째로 버려지고, 누적도 안 되어 경계도가 영원히 안 내려간다.
	// 매 프레임 값이 바뀌어도 복제는 늘지 않는다 — 양자화 값이 한 칸 움직일 때만 나간다
	const bool bGaugeChanged = (Clamped != AlertGauge);
	const bool bLevelChanged = (NextLevel != AlertLevel);

	if (!bGaugeChanged && !bLevelChanged)
	{
		return;
	}

	const EAlertLevel OldLevel = AlertLevel;

	AlertGauge = Clamped;
	AlertLevel = NextLevel;

	// 서버에서는 RepNotify 가 자동으로 안 불린다.
	// 리슨 서버의 호스트 창 HUD 도 갱신되려면 직접 불러야 한다
	if (bGaugeChanged)
	{
		const uint8 Quantized = Gauge01::Quantize(AlertGauge);
		if (Quantized != ReplicatedGauge)
		{
			ReplicatedGauge = Quantized;
			OnRep_ReplicatedGauge();
		}
	}
	if (bLevelChanged)
	{
		UE_LOG(LogAlert, Log, TEXT("경계도 단계 전환: %s -> %s (%.0f%%)"),
				*UEnum::GetValueAsString(OldLevel), *UEnum::GetValueAsString(AlertLevel), AlertGauge * 100.f);

		OnRep_AlertLevel();
	}

	if (AlertLevel == EAlertLevel::Alarm || AlertGauge <= 0.f)
	{
		SetComponentTickEnabled(false);   // 경보는 안 내려가고, 0 은 더 내려갈 곳이 없다
	}
}

void UAlertComponent::OnRep_ReplicatedGauge()
{
	// 클라에는 양자화된 값만 도착한다. 서버의 AlertGauge 는 이미 정밀한 원본이므로 덮지 않는다
	if (!HasServerAuthority(this))
	{
		AlertGauge = Gauge01::Dequantize(ReplicatedGauge);
	}

	OnAlertGaugeChanged.Broadcast(AlertGauge);
}

void UAlertComponent::OnRep_AlertLevel()
{
	// 복제로는 "바뀌었다"는 사실만 오고 이전 값은 오지 않는다.
	// 마지막으로 알린 값을 들고 있어야 구독자가 서버·클라 양쪽에서 같은 OldLevel 을 받는다.
	// 서버도 이 함수를 거치므로(ApplyGauge) 양쪽 경로가 하나로 합쳐진다
	const EAlertLevel PreviousLevel = LastBroadcastLevel;
	LastBroadcastLevel = AlertLevel;

	if (PreviousLevel != AlertLevel)
	{
		OnAlertLevelChanged.Broadcast(AlertLevel, PreviousLevel);
	}
}

APlayerState* UAlertComponent::GetNoisiestPlayer(float& OutContribution) const
{
	// 복제된 값을 그대로 쓴다. 서버에서 맵을 훑고 클라에서 이걸 읽는 식으로 갈라두면
	// 두 경로가 서로 다른 답을 내는 순간을 아무도 못 잡는다
	if (!IsValid(NoisiestPlayer))
	{
		OutContribution = 0.f;
		return nullptr;
	}

	OutContribution = NoisiestContribution;
	return NoisiestPlayer;
}

void UAlertComponent::SetAlertGauge01(float NewGauge)
{
	if (!HasServerAuthority(this))
	{
		return;
	}

	ApplyGauge(NewGauge);
	SetComponentTickEnabled(AlertGauge > 0.f && AlertLevel != EAlertLevel::Alarm);
}

void UAlertComponent::ResetAlert()
{
	if (!HasServerAuthority(this))
	{
		return;
	}

	AlertLevel         = EAlertLevel::Calm;   // 래치 해제. ApplyGauge 보다 먼저여야 한다
	SilenceTimer       = 0.f;
	PursuitCount       = 0;                   // 증원까지 남은 추격 수도 같이 되돌린다
	NoiseDetectionCount = 0;                  // 증원까지 남은 소음 감지 수도 같이 되돌린다
	ReinforcementCount = 0;					  // 증원 상환 되돌림.

	NoiseContribution.Reset();
	NoisiestPlayer       = nullptr;     // 복제본도 같이 비운다
	NoisiestContribution = 0.f;

	ApplyGauge(0.f);

	// 위에서 AlertLevel 을 이미 Calm 으로 바꿔놨으니 ApplyGauge 는 전환을 감지하지 못한다.
	// LastBroadcastLevel 이 예전 값을 들고 있어서 여기서 알린다
	OnRep_AlertLevel();

	SetComponentTickEnabled(false);
}

void UAlertComponent::ReportPursuitStarted()
{
	if (!HasServerAuthority(this))
	{
		return;
	}

	ReportTowardReinforcement(PursuitCount, UAlertSettings::Get()->PursuitsPerReinforcement, TEXT("경비 추격 발생"));
}

void UAlertComponent::ReportNoiseDetected()
{
	if (!HasServerAuthority(this))
	{
		return;
	}

	ReportTowardReinforcement(NoiseDetectionCount, UAlertSettings::Get()->NoiseDetectionsPerReinforcement, TEXT("경비 소음 감지"));
}

void UAlertComponent::ReportTowardReinforcement(int32& Counter, int32 Threshold, const TCHAR* SourceLabel)
{
	++Counter;
	Threshold = FMath::Max(1, Threshold);

	UE_LOG(LogAlert, Log, TEXT("%s (%d/%d) - 병력 증원까지"), SourceLabel, Counter, Threshold);

	if (Counter < Threshold)
	{
		return;
	}

	Counter = 0;

	// 증원 상한
	const int32 Cap = UAlertSettings::Get()->MaxReinforcements;
	if (Cap > 0 && ReinforcementCount >= Cap)
	{
		UE_LOG(LogAlert, Verbose, TEXT("병력 증원 상한(%d) 도달 — %s 무시."), Cap, SourceLabel);
		return;
	}

	++ReinforcementCount;

	UE_LOG(LogAlert, Log, TEXT("병력 증원 발동 (%d번째, 계기: %s) - 모든 경비 스포너에 1명씩 추가 스폰."),
		ReinforcementCount, SourceLabel);
	OnReinforcementTriggered.Broadcast(ReinforcementCount);
}

// ──────────────────────────────────────────────────────────────
// 자연 감소
// ──────────────────────────────────────────────────────────────

float UAlertComponent::GetSilenceGrace() const
{
	const UAlertSettings& Settings = *UAlertSettings::Get();
	return (AlertLevel == EAlertLevel::Alerted) ? Settings.AlertedSilenceSeconds : Settings.SilenceSeconds;
}

void UAlertComponent::TickComponent(float DeltaTime, ELevelTick TickType,
									FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasServerAuthority(this) || AlertLevel == EAlertLevel::Alarm || AlertGauge <= 0.f)
	{
		SetComponentTickEnabled(false);
		return;
	}

	SilenceTimer += DeltaTime;
	if (SilenceTimer < GetSilenceGrace())
	{
		return;
	}

	ApplyGauge(AlertGauge - UAlertSettings::Get()->DecayPerSecond * DeltaTime);
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 치트. 쉬핑 빌드에서는 코드째 빠진다 —
// hh.Alert.Set 0 은 경보를 지우는 치트라 출시 빌드 콘솔에 남아 있으면 안 된다
// ──────────────────────────────────────────────────────────────
#if !UE_BUILD_SHIPPING

static void AlertSetCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (World->IsNetMode(NM_Client))
	{
		UE_LOG(LogAlert, Warning, TEXT("경계도는 서버 권위입니다. 클라이언트에서는 바뀌지 않습니다."));
		return;
	}

	UAlertComponent* Alert = UAlertComponent::Get(World);
	if (!Alert)
	{
		UE_LOG(LogAlert, Warning, TEXT("GameState 에 AlertComponent 가 없습니다."));
		return;
	}

	const float Percent = Args.IsValidIndex(0) ? FCString::Atof(*Args[0]) : 0.f;
	Alert->SetAlertGauge01(Percent / 100.f);

	UE_LOG(LogAlert, Log, TEXT("경계도를 %.0f%% 로 설정했습니다."), Percent);
}

static FAutoConsoleCommandWithWorldAndArgs GAlertSetCommand(
	  TEXT("hh.Alert.Set"),
	  TEXT("hh.Alert.Set <퍼센트> — 경계도를 강제 설정한다. 예: hh.Alert.Set 66"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AlertSetCommand),
	  ECVF_Cheat);

// 경보는 래치라 hh.Alert.Set 0 으로는 안 풀린다. 테스트를 반복하려면 이게 필요하다
static void AlertResetCommand(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (World->IsNetMode(NM_Client))
	{
		UE_LOG(LogAlert, Warning, TEXT("경계도는 서버 권위입니다. 클라이언트에서는 바뀌지 않습니다."));
		return;
	}

	UAlertComponent* Alert = UAlertComponent::Get(World);
	if (!Alert)
	{
		UE_LOG(LogAlert, Warning, TEXT("GameState 에 AlertComponent 가 없습니다."));
		return;
	}

	Alert->ResetAlert();
	UE_LOG(LogAlert, Log, TEXT("경계도를 초기화했습니다 (경보 래치 · 소음 유발자 집계 포함)."));
}

static FAutoConsoleCommandWithWorld GAlertResetCommand(
	  TEXT("hh.Alert.Reset"),
	  TEXT("경계도를 0 으로 되돌리고 경보 래치를 푼다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&AlertResetCommand),
	  ECVF_Cheat);

// 임계값 비교가 경계에서 어긋날 때 원인을 가리기 위한 덤프.
// float 는 화면 표시로는 같아 보여도 비트가 다를 수 있어서 원시 비트까지 찍는다
static void AlertDumpCommand(const TArray<FString>& Args, UWorld* World)
{
	// Get() 은 CDO 라 null 이 아니다 (AlertSettings.h). 프로덕션 경로와 규칙을 맞춘다
	const UAlertSettings* Settings = UAlertSettings::Get();

	auto BitsOf = [](float Value) -> uint32
	{
		uint32 Raw = 0;
		FMemory::Memcpy(&Raw, &Value, sizeof(Raw));
		return Raw;
	};

	UE_LOG(LogAlert, Log, TEXT("SuspiciousEnter = %.9f  (0x%08X)"), Settings->SuspiciousEnter, BitsOf(Settings->SuspiciousEnter));
	UE_LOG(LogAlert, Log, TEXT("SuspiciousExit  = %.9f  (0x%08X)"), Settings->SuspiciousExit,  BitsOf(Settings->SuspiciousExit));
	UE_LOG(LogAlert, Log, TEXT("AlertedEnter    = %.9f  (0x%08X)"), Settings->AlertedEnter,    BitsOf(Settings->AlertedEnter));
	UE_LOG(LogAlert, Log, TEXT("AlertedExit     = %.9f  (0x%08X)"), Settings->AlertedExit,     BitsOf(Settings->AlertedExit));

	// 콘솔 명령이 실제로 만드는 값을 그대로 재현한다
	const FString Input  = Args.IsValidIndex(0) ? Args[0] : TEXT("67");
	const float   Parsed = FCString::Atof(*Input);
	const float   Gauge  = Parsed / 100.f;

	UE_LOG(LogAlert, Log, TEXT("입력 \"%s\" -> Atof %.9f -> /100 = %.9f  (0x%08X)"),
			*Input, Parsed, Gauge, BitsOf(Gauge));

	// 원시 비교와 실제 판정을 나란히 찍는다. 둘이 다르면 그게 곧 /fp:fast 오차다.
	// 판정에 쓰이는 것은 AtLeast 쪽이므로 원시 비교가 false 여도 정상일 수 있다
	UE_LOG(LogAlert, Log, TEXT("AlertedEnter    — 원시 >= %s / 실제판정 AtLeast %s"),
			(Gauge >= Settings->AlertedEnter)    ? TEXT("true ") : TEXT("false"),
			AtLeast(Gauge, Settings->AlertedEnter)    ? TEXT("true") : TEXT("false"));
	UE_LOG(LogAlert, Log, TEXT("SuspiciousEnter — 원시 >= %s / 실제판정 AtLeast %s"),
			(Gauge >= Settings->SuspiciousEnter) ? TEXT("true ") : TEXT("false"),
			AtLeast(Gauge, Settings->SuspiciousEnter) ? TEXT("true") : TEXT("false"));
}

static FAutoConsoleCommandWithWorldAndArgs GAlertDumpCommand(
	  TEXT("hh.Alert.Dump"),
	  TEXT("hh.Alert.Dump [퍼센트] — 임계값과 입력값을 원시 비트까지 찍는다. 기본 67"),
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AlertDumpCommand),
	  ECVF_Cheat);

// 기획서 8장 "최다 소음 유발자" 집계 확인.
// Instigator 가 제대로 심어져 있지 않으면 이 목록이 비어 있다
static void AlertContributorsCommand(UWorld* World)
{
	if (!World)
	{
		return;
	}

	const UAlertComponent* Alert = UAlertComponent::Get(World);
	if (!Alert)
	{
		UE_LOG(LogAlert, Warning, TEXT("GameState 에 AlertComponent 가 없습니다."));
		return;
	}

	// 복제되는 1위를 먼저 찍는다. 상세 맵은 서버에만 있으므로, 이걸 뒤로 미루면
	// 클라에서 "집계가 비었다 → Instigator 를 확인하라" 는 엉뚱한 경고만 보고 끝난다
	float Top = 0.f;
	if (const APlayerState* Noisiest = Alert->GetNoisiestPlayer(Top))
	{
		UE_LOG(LogAlert, Log, TEXT("최다 소음 유발자: %s (%.1f%%)"), *Noisiest->GetPlayerName(), Top * 100.f);
	}
	else
	{
		UE_LOG(LogAlert, Warning,
				TEXT("최다 소음 유발자가 없습니다. 소음원 액터에 Instigator 가 설정돼 있는지 확인하세요."));
	}

	// 여기부터는 서버 전용 데이터다
	if (!HasServerAuthority(World))
	{
		UE_LOG(LogAlert, Log, TEXT("(플레이어별 상세 집계는 서버에만 있습니다. 서버 창에서 실행하세요)"));
		return;
	}

	const TMap<TObjectPtr<APlayerState>, float>& Contribution = Alert->GetNoiseContribution();
	if (Contribution.IsEmpty())
	{
		UE_LOG(LogAlert, Log, TEXT("플레이어별 집계가 비어 있습니다."));
		return;
	}

	UE_LOG(LogAlert, Log, TEXT("── 소음 유발자 집계 (누적 경계도 기여량) ──"));
	for (const TPair<TObjectPtr<APlayerState>, float>& Pair : Contribution)
	{
		UE_LOG(LogAlert, Log, TEXT("  %s : %.1f%%"),
				IsValid(Pair.Key) ? *Pair.Key->GetPlayerName() : TEXT("(사라진 플레이어)"),
				Pair.Value * 100.f);
	}
}

static FAutoConsoleCommandWithWorld GAlertContributorsCommand(
	  TEXT("hh.Alert.Contributors"),
	  TEXT("플레이어별 누적 소음 기여량과 최다 소음 유발자를 출력한다"),
	  FConsoleCommandWithWorldDelegate::CreateStatic(&AlertContributorsCommand),
	  ECVF_Cheat);
#endif   // !UE_BUILD_SHIPPING
