# 07. 테스트 · 디버깅 및 진단 전략

> 이 문서는 **커밋되어 확인 가능한 코드**를 기준으로 쓴다.

---

## 1. 이 문서의 범위

**여기서 정하는 것** — 무엇을 어떻게 테스트하는가, 콘솔 변수·로그·디버그 드로잉 규칙, 테스트 맵 운영.

**여기서 정하지 않는 것**

| 주제 | 문서 |
|---|---|
| 서버 권위 · 복제 규칙 자체 | [02-Networking.md](02-Networking.md) |
| 증상별 네트워크 버그 진단 | [02-Networking.md](02-Networking.md) 9장 |
| 로그 · 디버그 코드의 네이밍 | [05-Conventions.md](05-Conventions.md) |

### 이 프로젝트에서 버그가 드러나는 방식

이 게임의 버그는 **크래시로 오지 않는다.** 코드가 조용히 아무 일도 하지 않는다.

- 소음이 발행되지 않으면 → "경비가 소리를 못 듣는다"
- 복제를 빠뜨리면 → "클라이언트에서만 안 된다"
- BT 노드가 실패하면 → "경비가 가만히 서 있는다"

**컴파일 에러도 로그도 없다.** 이 문서의 규칙은 전부 그 침묵을 깨기 위한 것이다.

---

## 2. 멀티플레이 테스트가 기본이다

> **모든 기능은 처음부터 멀티 환경에서 테스트한다.**

### PIE 설정

```
Play 설정 → Number of Players : 2 이상
           → Run Under One Process : 해제
           → Net Mode : Play As Listen Server
```

### 호스트 창에서만 확인하지 않는다

**호스트는 지연이 0이기 때문에 물리 운반 동기화 문제가 100% 정상으로 보인다.**
서버 코드와 클라이언트 코드가 같은 프로세스, 같은 메모리에서 돌기 때문이다.

**반드시 클라이언트 창에서 확인한다.** 클라이언트 창에서 보지 않은 기능은
동작한다고 말할 수 없다.

### 싱글로 먼저 만들지 않는다

싱글플레이로 만들고 나중에 복제를 붙이는 방식은 **대부분 전면 재작성으로 끝난다.**
권위 판정과 OnRep 재실행 구조는 나중에 얹을 수 있는 것이 아니다
([02-Networking.md](02-Networking.md) 5장).

---

## 3. Automation Test

### 무엇을 테스트하는가

**조용히 깨지는 것만 테스트한다.** 화면을 보면 아는 것은 테스트하지 않는다.

`NoiseSpamFilterTest` 주석에 기준이 있다 —

> 이 필터는 밸런싱 값이 계속 바뀔 자리인데, 잘못 고쳐도 컴파일 에러도 로그도 없이
> "경비가 소리를 못 듣는다" 로만 드러난다. 그래서 순수 상태 머신으로 떼어내 두고
> (입력 시퀀스 → 발행 시퀀스) 를 여기서 못박는다.

테스트 대상이 되는 조건:

1. **월드 없이 검증 가능한 순수 함수 · 상태 머신** — 게이지 양자화, 스팸 필터, 단계 판정
2. **밸런싱으로 계속 바뀔 자리** — 임계값을 한 칸 만졌을 때 규칙이 깨지는지
3. **실패가 침묵하는 것**

### 현재 테스트 (6개)

| 테스트 이름 | 지키는 것 |
|---|---|
| `HeavyHanded.Alert.Thresholds.HysteresisNotInverted` | Exit < Enter. 뒤집히면 단계에 갇히거나 영영 못 들어간다 |
| `HeavyHanded.Alert.Quantization.RoundTripWithinHalfStep` | 양자화 왕복 오차가 반 스텝 이내 |
| `HeavyHanded.Noise.SpamFilter.RollingPropStaysAudible` | 구르는 물체가 계속 들린다 (경계도만 낮아진다) |
| `HeavyHanded.Noise.SpamFilter.FalloffCountsEmitsNotHits` | 감쇄가 충돌 횟수가 아니라 발행 횟수를 센다 |
| `HeavyHanded.Noise.SpamFilter.FlushOnDestroyKeepsLastNoise` | 파괴될 때 마지막 소음이 유실되지 않는다 |
| `HeavyHanded.Noise.SpamFilter.GoesIdleWhenQuiet` | 조용해지면 필터가 유휴 상태로 돌아간다 |

### 이름 규칙

```
HeavyHanded.<시스템>.<대상>.<검증 내용>
```

**검증 내용은 "무엇이 참이어야 하는가"로 쓴다.** `SpamFilterTest2` 가 아니라
`RollingPropStaysAudible` 이다. 테스트가 깨졌을 때 이름만 보고 무엇이 무너졌는지 알아야 한다.

### 파일 위치와 가드

- 위치 — `Source/HeavyHanded/Private/Tests/`
- 전체를 `#if WITH_DEV_AUTOMATION_TESTS` 로 감싼다
- 플래그 — `EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter`

### 실행 방법

**에디터** — `Window > Test Automation` → `HeavyHanded` 트리에서 선택

**커맨드라인**

```
UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests HeavyHanded; Quit" -unattended -nullrhi
```

시스템별로 좁히려면 `HeavyHanded.Noise` 처럼 접두사만 넘긴다.

> CI 자동 실행은 아직 없다. 병합 전에 각자 돌린다.

### 테스트는 프로덕션 코드를 검증해야 한다

**규칙을 테스트 쪽에 복제하면, 테스트가 프로덕션 코드가 아니라 자기 복사본을 검증하게 된다.**
그 상태에서는 회귀를 잡을 수 없다. 예전에 `Gauge01` 양자화 식이 두 컴포넌트와 테스트에
각자 복사본으로 있었고, 정확히 그 일이 일어났다 ([02-Networking.md](02-Networking.md) 6장).

> **TODO:** `AlertLevelTest` 가 아직 이 상태다. `UAlertComponent::EvaluateLevel` 이
> private 이라 직접 부를 수 없어서, 규칙을 테스트에 복제해 두고 "임계값 세팅이 규칙을 만족하는가"만
> 검증하고 있다. `EvaluateLevel` 을 `FAlertLevelRules` 같은 자유 함수로 빼면 직접 호출로 바꿀 것.

**검증 대상을 순수 함수로 떼어내면 테스트가 쉬워진다.** 이게 `FNoiseSpamFilter` 가
컴포넌트에서 분리되어 있는 이유다.

---

## 4. 콘솔 변수와 콘솔 명령

### 현재 목록

| 이름 | 종류 | 하는 일 |
|---|---|---|
| `hh.Noise.Debug` | CVar | `1` 소음 반경 구 / `2` 오클루전 트레이스까지 |
| `hh.Ability.Debug` | CVar | 어빌리티 트레이스·궤적 시각화 |
| `hh.Alert.Set <0~1>` | 명령 (치트) | 경계도 게이지를 직접 설정 |

### 이름 규칙

```
hh.<시스템>.<대상>
```

`hh.` 접두사가 있어야 콘솔에서 `hh.` 만 쳐도 프로젝트 치트가 전부 나온다.
**짧게 유지한다** — 길면 아무도 안 친다.

### 치트에는 반드시 `ECVF_Cheat` 와 쉬핑 가드를 건다

`AlertComponent.cpp` 주석 그대로 —

> `hh.Alert.Set 0` 은 경보를 지우는 치트라 **협동 잠입 게임의 출시 빌드 콘솔에 남아 있으면 안 된다.**

```cpp
#if !UE_BUILD_SHIPPING
// ... 등록. ECVF_Cheat 를 붙인다
#endif
```

**쉬핑에서 코드째 빠지게 하고**, 개발·테스트 빌드에서도 `ECVF_Cheat` 를 단다.

### 단계값을 쓴다

`0 / 1 / 2` 로 정보량을 늘린다. 별개 CVar 를 여러 개 만드는 것보다 외우기 쉽다.

```cpp
if (CVarNoiseDebug.GetValueOnGameThread() > 0) { /* 반경 구 */ }
if (CVarNoiseDebug.GetValueOnGameThread() > 1) { /* 오클루전 트레이스 */ }
```

### 서버 권위 기능은 클라이언트에서 안 된다고 알린다

```cpp
if (World->IsNetMode(NM_Client))
{
    UE_LOG(LogAlert, Warning, TEXT("경계도는 서버 권위입니다. 클라이언트에서는 바뀌지 않습니다."));
    return;
}
```

**조용히 실패하지 않게 한다.** 치트가 안 먹는데 이유를 모르면 그것도 버그 신고가 된다.

---

## 5. 로그 카테고리

### 현재 목록

| 카테고리 | 시스템 | 선언 |
|---|---|---|
| `LogNoise` | 소음 서브시스템 | `NoiseSubsystem.cpp` (static) |
| `LogNoiseEmitter` | 소음 발행 컴포넌트 | `NoiseEmitterComponent.cpp` (static) |
| `LogAlert` | 경계도 | `AlertComponent.cpp` (static) |
| `LogGuardAI` | 경비 AI | `AI/GuardTypes.h` (**extern**) |
| `LogInteract` | 상호작용 어빌리티 | `GAB_Interact.cpp` (static) |
| `LogHeavyUI` | UI 위젯 | `PerceptionMeterWidget.cpp` (static) |
| `LogNoiseTest` `LogNoiseProp` | 소음 테스트 액터 | 각 `.cpp` (static) |

### 기본은 `DEFINE_LOG_CATEGORY_STATIC`

**`.cpp` 안에서만 쓴다면 static 으로 둔다.** 헤더에 노출하면 아무나 그 카테고리로 로그를 찍어
카테고리 필터의 의미가 사라진다.

여러 파일이 공유해야 할 때만 헤더에 `DECLARE_LOG_CATEGORY_EXTERN` 을 둔다.
`LogGuardAI` 가 유일한 사례다 — BT 노드 7개가 전부 같은 카테고리로 찍어야 하기 때문이다.

### 조용히 실패하는 지점마다 로그를 남긴다

`GuardTypes.h` 주석이 이 원칙을 그대로 적고 있다 —

> 이 경로는 실패해도 예외가 나지 않고 **"가만히 서 있는다"로만 드러나므로,
> 조용히 return 하는 지점마다 이유를 남긴다.**

**early return 앞에 로그를 붙이는 습관을 들인다.** 특히 아래는 반드시.

- 권위 체크에 걸려 되돌아갈 때
- 참조가 null 이라 아무 일도 안 할 때
- 데이터를 못 찾았을 때 (태그, DataTable 행, Blackboard 키)

### 반복 경로에서는 한 번만 찍는다

물리 충돌은 낙하 1회에 `OnHit` 이 5~15회 온다. 여기서 매번 찍으면 로그가 잠긴다.

`UNoiseSubsystem` 은 없는 프로파일을 **태그당 한 번만** 경고한다 (`WarnedMissingProfiles`).
같은 상황이면 같은 패턴을 쓴다.

---

## 6. 디버그 드로잉

### `ENABLE_DRAW_DEBUG` 로 감싼다

```cpp
#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), Event.Location, Event.Radius, 16, FColor::Yellow, false, DebugSeconds);
#endif
```

**`ENABLE_DRAW_DEBUG` 가 0이면 그리기 코드 자체가 컴파일에서 빠진다.**
런타임 `if` 로만 막으면 쉬핑 빌드에 코드가 남는다.

### 토글 기준 — 두 방식을 목적에 따라 쓴다

| 대상 | 방식 | 이유 | 사례 |
|---|---|---|---|
| **전역 시스템** (서브시스템, 단일 컴포넌트) | `hh.*` CVar | 인스턴스가 하나뿐이라 전역 스위치로 충분하고, 콘솔에서 즉시 토글된다 | `hh.Noise.Debug` `hh.Ability.Debug` |
| **인스턴스가 여럿인 액터** | 액터별 `bool` (`EditAnywhere`) | 레벨에 수십 개가 깔린다. 전부 켜면 화면이 덮인다 — **그중 하나만 보고 싶다** | `ALootBase::bShowImpactDebug` |

**액터별 스위치를 쓸 때는 getter 로 열어 하위 컴포넌트가 같은 스위치를 따르게 한다.**
`ULootDurabilityComponent` 가 `ALootBase::IsImpactDebugEnabled()` 를 보는 방식이다.
컴포넌트마다 별도 스위치를 두면 반쪽만 켜진 상태가 생긴다.

### 출력에 하한을 건다

디버그를 켠 순간 화면이 덮이면 안 켠 것과 같다.

`ALootBase::DebugMinLogImpulse` 는 이 값 미만의 충격을 출력에서 뺀다.
미세 진동·재접촉이 초당 수십 줄을 만들기 때문이다.

### 지속 시간은 데이터로 뺀다

`UNoiseSettings::DebugDrawDuration` — 하드코딩하면 매번 빌드해야 조정된다.

---

## 7. 테스트 맵과 테스트 액터

### 테스트 맵

각 시스템을 **격리된 환경에서** 검증한다. 정식 레벨에서만 테스트하면 다른 사람 작업에 막혀
검증 자체가 안 된다.

| 맵 | 검증 대상 | 담당 |
|---|---|---|
| `L_NoiseTest` | 소음 전파 · 감쇄 · 차폐 | 지성인 |
| `GuardTest` | 경비 순찰 · 조사 · 추격 | 유정석 |
| `LootTestLevel` | 노획물 물리 · 파손 임계값 | 김민준 |
| `Untitled` | 캐릭터 · GAS | 전영배 |

개인 테스트 맵은 `Content/Developers/<본인이름>/` 에 둔다
([05-Conventions.md](05-Conventions.md) 10장).

### 테스트 전용 액터

`Source/HeavyHanded/Public/Noise/` 에 소음 검증용 액터 3종이 있다.

| 클래스 | 하는 일 |
|---|---|
| `ANoiseTestProp` | 소음을 내는 물체. 클라이언트 창에서도 굴러가도록 이동 복제를 켠다 |
| `ANoiseTestListener` | `INoiseListener` 구현체. 들은 소음을 화면에 표시한다 |
| `ANoiseTestBlocker` | 차폐물. 오클루전 감쇄를 눈으로 확인한다 |

**테스트 액터는 프로덕션 클래스에 섞지 않는다.** 별도 클래스로 두고 이름에 `Test` 를 넣는다.
다른 시스템도 같은 방식으로 만들면 된다.

---

## 8. 이 문서를 고쳐야 할 때

- 새 Automation Test 를 만들면 3장 표에 **지키는 것**을 한 줄로 적는다
- 새 CVar · 로그 카테고리를 만들면 4·5장 표에 추가한다
- 조용히 실패하는 새 경로를 발견하면 **로그를 심고**, 재발 방지가 필요하면 테스트를 만든다
- CI 를 도입하면 3장 실행 방법에 추가한다
