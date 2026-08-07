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
#include "Noise/NoiseSubsystem.h"
#include "Noise/NoiseTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlert, Log, All);

namespace
{
	/**
	 * 임계값 비교 여유. float 연산 오차 한두 ULP 를 흡수한다.
	 *
	 * UE 는 게임 모듈을 /fp:fast 로 빌드한다 (VCToolChain.cs — FPSemanticsMode.Default).
	 * 이 모드에서 컴파일러는 x / 100.f 를 x * 0.01f 로 바꿀 수 있는데,
	 * 0.01f 가 0.01 보다 미세하게 작아서 67 을 곱하면 0.669999957 이 나온다.
	 * 리터럴 0.67f(0.670000017) 보다 1 ULP 낮아서 "67% 인데 경계 단계가 아닌" 상태가 된다.
	 *
	 * 히스테리시스 간격(4%p = 0.04)보다 400 배 작아서 단계 판정에는 영향을 주지 않는다.
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

	SetIsReplicatedByDefault(true);
}

void UAlertComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAlertComponent, AlertGauge);
	DOREPLIFETIME(UAlertComponent, AlertLevel);
}

bool UAlertComponent::HasAlertAuthority() const
{
	return GetOwnerRole() == ROLE_Authority;
}

void UAlertComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAlertAuthority())
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
	if (!HasAlertAuthority() || AlertLevel == EAlertLevel::Alarm)
	{
		return;   // 경보는 래치라 더 올릴 것도 없다
	}

	// 소리 크기에 비례해 오른다. 7단계 공식: AlertDelta = Row.AlertDelta * Loudness01
	const float Delta = Profile.AlertDelta * Event.Loudness01;

	SilenceTimer = 0.f;   // 소음이 나면 무소음 타이머 리셋

	if (Delta <= 0.f)
	{
		return;
	}

	// 결과 화면 "최다 소음 유발자" 집계 (기획서 8장).
	// 여기서 안 모으면 나중에 소음 이벤트를 다시 파고들어야 한다
	if (APlayerState* PlayerState = ResolvePlayerState(Instigator))
	{
		NoiseContribution.FindOrAdd(PlayerState) += Delta;
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

	const UAlertSettings* Settings = UAlertSettings::Get();
	if (!Settings)
	{
		return Current;
	}

	// 진입은 Enter, 이탈은 Exit 임계값을 쓴다. 이 간격(히스테리시스)이 없으면
	// 33/34% 경계에서 단계가 깜빡이면서 셔터가 열렸다 닫혔다 한다.
	//
	// 한 번에 두 단계를 건너뛰는 경우도 처리한다 — 평온에서 특대 소음 한 방에
	// 67% 를 넘으면 의심을 거치지 않고 바로 경계로 가야 한다
	switch (Current)
	{
	case EAlertLevel::Calm:
		if (AtLeast(Gauge, Settings->AlertedEnter))    { return EAlertLevel::Alerted; }
		return AtLeast(Gauge, Settings->SuspiciousEnter) ? EAlertLevel::Suspicious : EAlertLevel::Calm;

	case EAlertLevel::Suspicious:
		if (AtLeast(Gauge, Settings->AlertedEnter))    { return EAlertLevel::Alerted; }
		return AtLeast(Gauge, Settings->SuspiciousExit) ? EAlertLevel::Suspicious : EAlertLevel::Calm;

	case EAlertLevel::Alerted:
		if (!AtLeast(Gauge, Settings->SuspiciousExit)) { return EAlertLevel::Calm; }
		return AtLeast(Gauge, Settings->AlertedExit) ? EAlertLevel::Alerted : EAlertLevel::Suspicious;

	default:
		return Current;
	}
}

void UAlertComponent::ApplyGauge(float NewGauge)
{
	const float Clamped = FMath::Clamp(NewGauge, 0.f, 1.f);
	const EAlertLevel NextLevel = EvaluateLevel(Clamped, AlertLevel);

	const bool bGaugeChanged = !FMath::IsNearlyEqual(Clamped, AlertGauge);
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
		OnRep_AlertGauge();
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

void UAlertComponent::OnRep_AlertGauge()
{
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
	APlayerState* Noisiest = nullptr;
	OutContribution = 0.f;

	for (const TPair<TObjectPtr<APlayerState>, float>& Pair : NoiseContribution)
	{
		if (Pair.Value > OutContribution && IsValid(Pair.Key))
		{
			Noisiest        = Pair.Key;
			OutContribution = Pair.Value;
		}
	}

	return Noisiest;
}

void UAlertComponent::SetAlertGauge01(float NewGauge)
{
	if (!HasAlertAuthority())
	{
		return;
	}

	ApplyGauge(NewGauge);
	SetComponentTickEnabled(AlertGauge > 0.f && AlertLevel != EAlertLevel::Alarm);
}

void UAlertComponent::ResetAlert()
{
	if (!HasAlertAuthority())
	{
		return;
	}

	AlertLevel   = EAlertLevel::Calm;   // 래치 해제. ApplyGauge 보다 먼저여야 한다
	SilenceTimer = 0.f;
	NoiseContribution.Reset();

	ApplyGauge(0.f);

	// 위에서 AlertLevel 을 이미 Calm 으로 바꿔놨으니 ApplyGauge 는 전환을 감지하지 못한다.
	// LastBroadcastLevel 이 예전 값을 들고 있어서 여기서 알린다
	OnRep_AlertLevel();

	SetComponentTickEnabled(false);
}

// ──────────────────────────────────────────────────────────────
// 자연 감소
// ──────────────────────────────────────────────────────────────

float UAlertComponent::GetSilenceGrace() const
{
	const UAlertSettings* Settings = UAlertSettings::Get();
	if (!Settings)
	{
		return 30.f;
	}

	return (AlertLevel == EAlertLevel::Alerted) ? Settings->AlertedSilenceSeconds : Settings->SilenceSeconds;
}

void UAlertComponent::TickComponent(float DeltaTime, ELevelTick TickType,
																	  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasAlertAuthority() || AlertLevel == EAlertLevel::Alarm || AlertGauge <= 0.f)
	{
		SetComponentTickEnabled(false);
		return;
	}

	SilenceTimer += DeltaTime;
	if (SilenceTimer < GetSilenceGrace())
	{
		return;
	}

	const UAlertSettings* Settings = UAlertSettings::Get();
	const float DecayPerSecond = Settings ? Settings->DecayPerSecond : 0.01f;

	ApplyGauge(AlertGauge - DecayPerSecond * DeltaTime);
}

// ──────────────────────────────────────────────────────────────
// [디버그 전용] 치트
// ──────────────────────────────────────────────────────────────
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
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AlertSetCommand));

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
	  FConsoleCommandWithWorldDelegate::CreateStatic(&AlertResetCommand));

// 임계값 비교가 경계에서 어긋날 때 원인을 가리기 위한 덤프.
// float 는 화면 표시로는 같아 보여도 비트가 다를 수 있어서 원시 비트까지 찍는다
static void AlertDumpCommand(const TArray<FString>& Args, UWorld* World)
{
	const UAlertSettings* Settings = UAlertSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogAlert, Warning, TEXT("UAlertSettings CDO 를 가져오지 못했습니다."));
		return;
	}

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
	  FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AlertDumpCommand));