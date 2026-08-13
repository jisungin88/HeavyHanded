# 경비 AI — 순찰 · 조사 · 추격

| | |
|---|---|
| **담당** | 유정석 |
| **코드 위치** | ⚠️ **`feature/AI`** — `develop` 에 아직 병합되지 않았다 |
| **소스** | `Public|Private/AI/`, `.../Character/GuardCharacter.h` |
| **태그** | `Config/Tags/Guard.ini` |
| **테스트** | `GuardTest` (`Content/HeavyHanded/Maps/Test/`) |

> **이 문서는 골격이다.** 표에 확인된 사실만 채워져 있고, 설명은 담당자가 채운다.
> 작성 형식은 [Noise-Alert.md](Noise-Alert.md) 를 참고할 것.

---

## 1. 이 시스템이 하는 일

기획서 6장 — 순찰 위협 3종(경비견 · 경비원 · 무장 경비), 3장 — 인지 게이지 이후의 조사 행동.

> **TODO:** 한 문단으로. 특히 소음 파트와의 경계(인지 게이지 누적은 저쪽, 100% 이후 행동은 이쪽)를 적을 것.

---

## 2. 클래스 구성

| 클래스 | 부모 | 역할 |
|---|---|---|
| `AGuardAIController` | `AAIController` | 순찰 · 조사 지점 선택, Perception 콜백, Blackboard 갱신 |
| `AGuardCharacter` | `ACharacter` | 경비 폰. `UPerceptionMeterComponent` 보유 |

### BT 노드 (7종)

| 노드 | 종류 |
|---|---|
| `BTTask_SelectNextPatrolPoint` | Task |
| `BTTask_SelectSearchPoint` | Task |
| `BTTask_MoveToInvestigate` | Task (래턴트) |
| `BTDecorator_CheckDetectionGauge` | Decorator (옵저버) |
| `BTDecorator_CheckSearchTimeout` | Decorator |
| `BTDecorator_CheckWorldAlert` | Decorator |
| `BTService_UpdateDetectionGauge` | Service |

**Blackboard 키 상수는 `AI/GuardBlackboardKeys.h` 에 모여 있다.** 문자열로 흩어 쓰지 않는다.

### 타입

| 타입 | 값 |
|---|---|
| `EGuardType` | `Standard` / `Dog` / `Armed` |
| `EPatrolPattern` | `Loop` / `PingPong`(권장) / `Random` |

### BP 에셋

`BT_Guards`, `BB_Guards`, `BP_GuardBase`, `BP_GuardAIController`,
`BP_GuardTestGameMode`, `BP_GuardTestPlayer`, `IMC_GuardTest`

> **BT 구조는 에셋, 판정 노드는 C++.** 새 판정이 필요하면 C++ 노드를 만들고 트리에서 조립만 한다
> ([03-CppVsBlueprint.md](../03-CppVsBlueprint.md) 7장).

---

## 3. 핵심 흐름

> **TODO:** 상태 전이도(순찰 ↔ 조사 ↔ 추격)와 각 전이 조건. BT 브랜치 구조와 대응시켜 적을 것.

### 순찰

`SelectNextPatrolPoint()` 는 **아직 현재 목표에 도착하지 않았다면 지점을 넘기지 않는다.**
브랜치 진입마다 불리는데, 시야 획득/상실로 순찰이 abort 됐다 재개될 때마다 지점을 건너뛰면
순찰 경로가 망가지기 때문이다.

`PatrolArrivalRadius` 는 **BT `Move To` 노드의 `Acceptable Radius` 보다 조금 크게** 잡아야 한다.
작으면 도착 판정이 안 나 같은 지점을 무한히 다시 지정한다.

### 조사

`[마지막 목격 지점]` → `[주변 무작위 지점 × SearchSweepCount]` 순서로 진행한다.
더 훑을 지점이 없으면 `false` 를 돌려주고 태스크가 `Failed` 로 브랜치를 끝내 순찰로 돌려보낸다.

조사 세션은 `SearchStartTime` 값으로 구분한다. 그 값이 바뀌면 새 조사로 보고 훑기 횟수를 초기화한다.

---

## 4. 데이터와 수치

| 무엇 | 어디 |
|---|---|
| 경비 종류 · 상태 · 감지 태그 | `Config/Tags/Guard.ini` |
| 시야 · 청각 파라미터 | `SightConfig` / `HearingConfig` (BP 에서 `EGuardType` 별로 덮어쓴다) |
| 순찰 도착 반경, 수색 횟수 · 반경 | `AGuardAIController` `EditAnywhere` |
| 인지 게이지 파라미터 | `UPerceptionMeterComponent` ([Noise-Alert.md](Noise-Alert.md) 7장) |

> Perception 파라미터를 멤버로 들고 있는 이유 — 지역 변수로 두면 디테일 패널에 뜨지 않아
> 반경·시야각을 전혀 조정할 수 없다.

---

## 5. 네트워크

경비 AI 는 **서버에서만 돈다.** BT · Blackboard · Perception 모두 서버 전용이다.
클라이언트에 필요한 것은 `UPerceptionMeterComponent::ReplicatedPerception`(머리 위 게이지)뿐이다.

> **TODO:** 경비 이동의 클라이언트 동기화 방식을 확인해 적을 것.

---

## 6. 다른 시스템과의 접점

| 상대 | 방향 | 수단 |
|---|---|---|
| 소음 (지성인) | ← 이쪽 | `UPerceptionMeterComponent::OnPerceptionFull` 구독 |
| 소음 (지성인) | ← 이쪽 | 경계도 조회 (`AGuardAIController::GetWorldAlertLevel`) |
| 플레이어 (전영배) | 양방향 | 시야 포착, 접촉 시 다운 |
| 환경 방해 요소 (오유석) | ← 이쪽 | 미끼 유인, 카메라 · 센서 연동 |

**조사 종료 후 `ResetPerception()` 을 반드시 호출한다.**
없으면 경비가 영원히 100% 에 박혀 있다. 현재 `HandlePerceptionFull()` 이 호출한다.

---

## 7. 디버깅

| 수단 | 비고 |
|---|---|
| `LogGuardAI` | **이 시스템 전용 로그 카테고리** (`AI/GuardTypes.h` 에 `EXTERN`) |

> 이 경로는 실패해도 예외가 나지 않고 **"가만히 서 있는다"로만 드러나므로,
> 조용히 return 하는 지점마다 이유를 남긴다.**

BT 노드 7개가 전부 같은 카테고리로 찍어야 해서 헤더에 `EXTERN` 으로 둔 유일한 사례다
([07-Testing.md](../07-Testing.md) 5장).

---

## 8. 알려진 제약과 TODO

> **TODO:** `AGuardAIController::GetWorldAlertLevel()` 이 **임시 GameState 변수**를 읽고 있다.
> 코드 주석에 "정식 `UAlertComponent` 로 교체 시 이 함수 내부만 수정하면 된다"고 되어 있다.
> `UAlertComponent::Get(WorldContext)` 로 교체할 것.

> **TODO:** `EGuardType` 3종 중 `Dog` · `Armed` 의 고유 동작(경비견 3회 접촉, 무장 경비 접촉 시
> 즉시 다운)이 미착수. `Guard.ini` 에 태그만 정의되어 있다.

> **TODO:** `Guard.State.Distracted`(미끼 유인), `Guard.State.Deceived`(변장 인식 실패)는
> 태그만 있고 구현이 없다. 장비 · 미믹 스킬이 붙을 때 연결한다.

> **TODO:** 경비 시야 포착으로 인한 경계도 상승(`Alert.Source.Vision`)이 연결되어 있지 않다
> ([Noise-Alert.md](Noise-Alert.md) 11장).
