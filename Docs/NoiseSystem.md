# 소음 · 경계도 시스템

담당: 김지성 · 브랜치 `feature/NoiseAndBoundary`
기획서 3장(소음/경계도) · 8장(결과 화면 최다 소음 유발자)

**원칙: 다른 사람은 내 파일을 수정하지 않고 호출만 한다. 나도 다른 사람 클래스를 수정하지 않고 컴포넌트를 준다.**

---

## 1. 구조

```
소음 발생                 UNoiseSubsystem              소비
─────────                ────────────────             ────
UNoiseEmitterComponent ─┐
  물리 충돌 자동         │
  ReportTaggedNoise()   ├─→ ReportNoise() ─┬─→ OnNoiseReported ──→ UAlertComponent
                        │                  │    (감쇄 전, 전 구역)    경계도 · 소음 유발자 집계
직접 호출               ─┘                  │
  ReportNoise()                            └─→ Propagate() ───────→ INoiseListener
                                                거리·차폐 감쇄        UPerceptionMeterComponent
StartContinuousNoise()                                                (경비 인지 게이지)
  4Hz 티커
```

**소음은 서버 100% 권위입니다.** 클라이언트에서 호출하면 조용히 무시됩니다 — 호출부가 `HasAuthority()` 를 안 써도 되게 한 것입니다.

| 클래스 | 위치 | 역할 |
|---|---|---|
| `UNoiseSubsystem` | WorldSubsystem | 발행 · 전파 · 감쇄. 진입점 |
| `UNoiseEmitterComponent` | 노획물 등에 부착 | 물리 충돌 → 소음 |
| `UPerceptionMeterComponent` | 경비에 부착 | 소음 자극 → 인지 게이지 |
| `UAlertComponent` | GameState에 **자동** 부착 | 저택 전체 경계도 4단계 |
| `INoiseListener` | 인터페이스 | 소음을 듣는 쪽 |

수치는 전부 데이터입니다. **C++ / BP 에 하드코딩 금지.**

- `DT_NoiseProfiles` (`Content/HeavyHanded/Data/DataTables/`) — 행동별 등급 · 경계도 · 반경 · 충격량 범위 · 쿨다운
- Project Settings → **Game → Noise** — 재질 계수, 감쇄 파라미터
- Project Settings → **Game → Alert** — 경계 단계 임계값, 자연 감소

---

## 2. 팀별 접점

| 팀원 | 담당 | 해야 할 것 |
|---|---|---|
| 이지은 | 세션 & 네트워크 | **없음.** `UAlertComponent` 가 GameState에 자동 부착됩니다 |
| 전영배 | 플레이어 조작, 스킬 · 패시브 | `ReportNoise` 호출, `AddModifier` 로 감쇄 등록 |
| 오유석 | 환경 방해 요소, 협력 장치 | `ReportNoise` 호출, `OnAlertLevelChanged` 구독 |
| 유정석 | AI | 경비에 `UPerceptionMeterComponent` 부착, `OnPerceptionFull` 구독 |
| 김민준 | 물리, 아이템 | 노획물에 `UNoiseEmitterComponent` 부착 |

---

## 3. API

### 소음 내기 — 전영배 · 오유석 · 김민준

```cpp
#include "Noise/NoiseSubsystem.h"

if (UNoiseSubsystem* Noise = UNoiseSubsystem::Get(this))
{
    Noise->ReportNoise(Tag, Location, /*LoudnessScale*/ 1.f, /*Instigator*/ this);
}
```

`Tag` 는 `DT_NoiseProfiles` 의 RowName 과 일치해야 합니다. 없으면 부모 태그로 폴백하고, 그것도 없으면 경고 후 무시됩니다.

**`Instigator` 를 꼭 넘기세요.** 결과 화면 "최다 소음 유발자" 집계가 이걸로 플레이어를 역추적합니다. 폰이 아니면 액터의 `Instigator` 컨트롤러를 따라가므로, 던져진 노획물은 **던질 때 `SetInstigator(던진 폰)`** 이 필요합니다.

### 지속 소음 — 김민준 (대형 금고 절단)

```cpp
FGuid Handle = Noise->StartContinuousNoise(Noise_Device_SafeCut, this);
// ...
Noise->StopContinuousNoise(Handle);   // 반드시 정지시킬 것
```

4Hz 로 발행됩니다. 소음원 액터가 파괴되면 자동 종료됩니다.

### 물리 충돌 소음 — 김민준

노획물에 `UNoiseEmitterComponent` 를 붙이면 충돌 소음은 **자동**입니다. 충돌이 아닌 경로만 직접 호출합니다.

```cpp
#include "Noise/NoiseEmitterComponent.h"

Emitter->ReportTaggedNoise(Noise_Loot_Throw, 1.f);   // 던지기 · 파괴 · 떨어뜨리기 · 유출
```

컴포넌트가 `SetNotifyRigidBodyCollision(true)` 를 알아서 켜므로 BP에서 체크할 필요 없습니다.
`ImpactComponentName` 이 비어 있으면 루트 프리미티브를 씁니다.

> **`DT_NoiseProfiles` 의 충돌 태그는 `MinImpulse > 0` 필수.**
> 물리 오브젝트는 **정지한 뒤에도 접촉 이벤트를 계속 발생**시킵니다. `MinImpulse` 가 이걸 막습니다.
> 0으로 두면 가만히 있는 노획물이 쿨다운마다 영원히 소음을 냅니다.

### 소음 감쇄 · 치환 — 전영배 (장비 · 패시브)

```cpp
FNoiseModifier Modifier;
Modifier.AffectedTags = FGameplayTagQuery::MakeQuery_MatchTag(Noise_Player);   // 비우면 전부
Modifier.Multiplier   = 0.5f;                                                  // 고무창 신발
FGuid Handle = Emitter->AddModifier(Modifier);
// ...
Emitter->RemoveModifier(Handle);
```

미믹 "발소리 위장" 처럼 배율이 아니라 태그를 갈아끼우려면 `Modifier.OverrideTag` 를 채웁니다.

### 경비 인지 게이지 — 유정석

경비에 `UPerceptionMeterComponent` 를 붙이고 구독합니다.

```cpp
PerceptionMeter->OnPerceptionFull.AddDynamic(this, &AGuard::StartInvestigate);
// void AGuard::StartInvestigate(FVector LastNoiseLocation)
```

| 함수 | 설명 |
|---|---|
| `GetPerception01()` | 0~1. 복제됨 |
| `IsLatched()` | 100% 도달 후 래치 상태인가 |
| `GetLastNoiseLocation()` | 조사 목적지 |
| `ResetPerception()` | **조사 종료 후 반드시 호출** |
| `OnPerceptionChanged(float)` | 머리 위 게이지 위젯용 |

> **`ResetPerception()` 을 안 부르면 경비가 그 소음 지점에 영구히 묶입니다.**
> 100% 에서 래치되며 이 함수만이 래치를 풉니다.

### 경계도 — 오유석 (셔터 · 경비 증원) · UI

```cpp
#include "Alert/AlertComponent.h"

if (UAlertComponent* Alert = UAlertComponent::Get(this))
{
    Alert->OnAlertLevelChanged.AddDynamic(this, &AMyShutter::HandleAlertLevel);
}
// void AMyShutter::HandleAlertLevel(EAlertLevel NewLevel, EAlertLevel OldLevel)
```

| 함수 | 설명 |
|---|---|
| `UAlertComponent::Get(WorldContext)` | GameState에 붙은 인스턴스. 없으면 nullptr |
| `GetAlertGauge01()` / `GetAlertLevel()` | 둘 다 복제됨 |
| `IsAlarmed()` | 경보(래치) 상태 |
| `GetNoisiestPlayer(float& Out)` | 결과 화면용 최다 소음 유발자 |
| `OnAlertGaugeChanged(float)` | HUD 게이지용 |

`EAlertLevel` — `Calm`(평온) / `Suspicious`(의심) / `Alerted`(경계) / `Alarm`(경보)

**히스테리시스가 있어서 단계는 게이지만으로 결정되지 않습니다.** 게이지 65%가 올라가는 중이면 의심, 내려가는 중이면 경계입니다. 직접 계산하지 말고 `GetAlertLevel()` 을 쓰세요.

경보는 **래치**라 자연 감소로 풀리지 않습니다. `ResetAlert()` 만이 풉니다.

### 소음 듣기 — 그 외

경비 말고도 소음에 반응할 게 있으면 `INoiseListener` 를 구현합니다.

```cpp
class AMyThing : public AActor, public INoiseListener
{
    virtual void OnNoiseHeard_Implementation(const FNoiseStimulus& Stimulus) override;
    virtual FVector GetListenerLocation_Implementation() const override;
};
// BeginPlay 에서 Subsystem->RegisterListener(this)
// EndPlay 에서 UnregisterListener(this)   ← 빠뜨리지 말 것
```

`FNoiseStimulus.Strength` 는 거리 · 차폐 감쇄가 모두 끝난 0~1 값입니다.

**위치 개념이 없는 구독자**(경계도, 집계 등)는 청취자가 아니라 `UNoiseSubsystem::OnNoiseReported`(감쇄 전, C++ 전용)를 씁니다.

---

## 4. 확정된 설계 결정 — 다시 논의하지 말 것

| 결정 | 선택 | 이유 |
|---|---|---|
| 경계도 게이지 소유 | 소음 담당 스코프에 포함 | 기획서 3장이 "소음과 경계도" 한 세트. 밸런싱이 한 곳에서 되어야 함 |
| 경비 인지 게이지 | 컴포넌트만 제공 | 누적 계산은 소음 쪽, 100% 이후 조사 행동/BT는 AI 쪽 |
| 감쇄 방식 | 라인트레이스 오클루전 | NavMesh 경로거리는 비용이 크고 문 개폐 반영이 까다로움 |
| 경계도 부착 위치 | GameState에 런타임 부착 | `GameStateSetEvent` 사용 — 남의 클래스를 안 건드림. 복제 검증 완료 |
| 소음 권위 | 서버 100% | 클라 물리 결과는 사람마다 달라서 신뢰 불가 |
| 게이지·단계 복제 | 둘 다 복제 | 히스테리시스 때문에 단계를 게이지에서 유도할 수 없음 |
| 모디파이어 | 인터페이스 아닌 값 구조체 | 다른 사람이 소음 코드를 상속·수정하지 않게 하려는 것 |
| 타입 프리픽스 | 없음 (`FNoiseEvent`, `EAlertLevel`) | 팀 컨벤션 2-2 표의 예시가 그대로 이 이름들 |

**선점한 타입 이름** — 중복 정의하지 마세요.
`FNoiseEvent` / `FNoiseStimulus` / `FNoiseProfileRow` / `FNoiseModifier` / `ENoiseGrade` / `EAlertLevel`

---

## 5. 디버그 콘솔 명령

전부 서버(호스트) 창에서 실행합니다. 클라에서는 경고만 뜹니다.

| 명령 | 설명 |
|---|---|
| `hh.Noise.Debug 0/1/2` | 1 = 소음 반경 구체, 2 = + 오클루전 트레이스 라인 |
| `hh.Noise.Test <태그> [크기]` | 플레이어 위치에 소음 발행. 예: `hh.Noise.Test Noise.Loot.Throw 1.0` |
| `hh.Noise.SpawnProp [높이]` | 물리 상자를 상공에 떨어뜨린다 (기본 500) |
| `hh.Noise.ClearProps` | 테스트 상자 제거 |
| `hh.Noise.ResetListeners` | 테스트 청취자의 인지 게이지 초기화 |
| `hh.Alert.Set <퍼센트>` | 경계도 강제 설정 |
| `hh.Alert.Reset` | 경계도 0 + 경보 래치 해제 + 집계 초기화 |
| `hh.Alert.Dump [퍼센트]` | 임계값과 입력값을 원시 비트까지 출력 |
| `hh.Alert.Contributors` | 플레이어별 누적 소음 기여량 + 최다 소음 유발자 |

충격량 튜닝에는 로그를 켭니다. `MinImpulse` / `MaxImpulse` 조정에 필요합니다.

```
Log LogNoiseEmitter Verbose
```

```
NoiseTestProp_0 충돌 — Impulse 60837.7 (Min 500.0 / Max 8000.0) -> Base01 1.000
발행: Noise.Loot.Impact Loudness 1.000 (연속 1회, 쿨다운 0.25s)
```

충돌 수와 발행 수가 같으면 스팸 필터가 새고 있는 것입니다.

**복제 확인은 클라 창에서** (코드 불필요):

```
obj list class=AlertComponent          # 클라에 인스턴스가 생겼는지
DisplayAll AlertComponent AlertGauge   # 값이 따라오는지 (DisplayClear 로 끔)
```

---

## 6. 열린 항목

### 팀 확인 필요

**`GameTraceChannel` 번호 배분.** 각자 잡으면 두 명이 같은 번호를 써서 원인 못 찾는 버그가 납니다.

```
GameTraceChannel1 = NoiseOcclusion   (김지성)  ← 커밋 완료
GameTraceChannel2 = ?                (유정석 — 경비 시야?)
GameTraceChannel3 = ?                (오유석 — 레이저/센서?)
GameTraceChannel4 = ?                (김민준 — 운반 판정?)
```

### 기획자 확인 필요

| 항목 | 현재 | 문제 |
|---|---|---|
| 물건 충돌 경계도 | `+1%` (기획서 3장) | 부록 시트는 `+10%`. 현재 값이면 **낙하 1회 1.5%, 경보까지 약 67회** |
| 경계 단계 무소음 타이머 | 소음 시 리셋 | 60초를 못 채우고 소음이 나면 리셋인지 누적인지 미정의 |
| 소음 동시 발생 | 각각 독립 가산 | 합산 규칙 미정의 |
| "고무창 신발 -50%" | 미적용 | 걷기는 원래 0% → 뛰기에만 적용되는 것인지 |

**확정되면 `DT_NoiseProfiles` 가 유일한 진리원입니다. 코드 수정은 필요 없습니다.**

### 콘텐츠 작업 대기

- **`PhysicalMaterial` 에셋 미생성** — `PM_Concrete` / `PM_Wood` / `PM_Carpet` … 지금은 전부 `SurfaceType_Default`(계수 1.0) 로 잡혀서 **재질별 차이가 하나도 안 납니다.** 에셋을 만들어 각 `SurfaceType` 을 지정하고 머티리얼에 물려야 Project Settings의 재질 계수 표가 살아납니다
- **`Impact Curve` 전부 `None`** (선형). 중간 구간 표현력이 없습니다
- **`MaxImpulse` 재조정** — 실제 노획물 질량이 정해진 뒤. 현재 40kg 상자를 5m에서 떨어뜨리면 충격량이 `MaxImpulse` 의 7.6배라 어떤 낙하든 최대 소음으로 포화됩니다

### 삭제 예정

실제 경비 · 노획물 액터가 들어오면 지웁니다.

- `ANoiseTestListener` / `ANoiseTestBlocker` / `ANoiseTestProp`
- 테스트 레벨 `Content/Developers/Ji/Maps/L_NoiseTest`
