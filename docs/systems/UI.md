# UI · HUD · 위젯

| | |
|---|---|
| **담당** | ⚠️ **미정** — 아래 TODO 참조 |
| **코드 위치** | ⚠️ **`feature/uiux`** — `develop` 에 아직 병합되지 않았다 |
| **소스** | `Public|Private/UI/` |
| **설정** | `Config/DefaultUI.ini` → Project Settings → **UI** |
| **에셋** | `Content/HeavyHanded/UI/` |

> **이 문서는 골격이다.** 표에 확인된 사실만 채워져 있고, 설명은 담당자가 채운다.
> 작성 형식은 [Noise-Alert.md](Noise-Alert.md) 를 참고할 것.

---

## 1. 이 시스템이 하는 일

기획서 8장 — HUD, 경비 인지 게이지, 소음 피드백, 상호작용 프롬프트, 결과 화면.

> **TODO:** 한 문단으로.

---

## 2. 클래스 구성

| 클래스 | 부모 | 역할 |
|---|---|---|
| `UAlertGaugeWidget` | `UUserWidget` (**Abstract**) | 경계도 게이지 데이터 바인딩 |
| `UPerceptionMeterWidget` | `UUserWidget` (**Abstract**) | 경비 머리 위 인지 게이지 |
| `UUISettings` | `UDeveloperSettings` | 디자인 토큰 (색상 등) |

### `Abstract` 인 이유

**C++ 클래스를 직접 스폰하면 화면에 아무것도 안 나온다.**
C++ 베이스가 구독 · 해제 · 데이터 바인딩을 하고, `WBP_*` 서브클래스가 비주얼을 전부 만든다.
BP 서브클래스를 강제하려고 `Abstract` 로 막았다
([03-CppVsBlueprint.md](../03-CppVsBlueprint.md) 4장).

### BP 연출 훅

| 함수 | 클래스 | 하는 일 |
|---|---|---|
| `OnGaugeUpdated` | `UAlertGaugeWidget` | 바 채우기와 보간 |
| `OnLevelUpdated` | `UAlertGaugeWidget` | 색 전환 · 펄스 · 경보 점멸. **최초 바인딩 시에도 한 번 호출되며 이때는 `NewLevel == OldLevel`** |
| `OnPerceptionUpdated` | `UPerceptionMeterWidget` | 원형 채우기와 보간 |
| `OnMeterVisibilityChanged` | `UPerceptionMeterWidget` | 페이드 인/아웃 |

### BP 에셋

`WBP_HUD`, `WBP_AlertGauge`, `WBP_PerceptionMeter`,
공통 — `WBP_Button`, `WBP_Card`, `WBP_StatBar`

로비 · 세션 위젯은 [Session.md](Session.md) 참조.

---

## 3. 핵심 흐름

> **TODO:** 위젯 생성 · 구독 · 해제 시점. `NativeConstruct` / `NativeDestruct` 에서
> 무엇을 바인딩하고 푸는지.

**UI 는 시스템 상태를 읽고 구독만 한다. 쓰지 않는다**
([01-Architecture.md](../01-Architecture.md) 3장 규칙 5).

---

## 4. 데이터와 수치

| 무엇 | 어디 |
|---|---|
| 디자인 토큰 (색상 등) | `UUISettings` → `Config/DefaultUI.ini` |
| 경계 단계별 색상 | `UUISettings` |

> **TODO:** `UUISettings` 에 현재 무엇이 있는지 정리. `DefaultUI.ini` 에는 `GoldColor` 하나만
> 저장되어 있어 나머지는 C++ 기본값을 쓰는 상태다.

---

## 5. 네트워크

UI 는 복제하지 않는다. 복제된 상태를 **읽기만** 한다.

| 구독 대상 | 출처 |
|---|---|
| `OnAlertGaugeChanged` / `OnAlertLevelChanged` | `UAlertComponent` |
| `OnPerceptionChanged` | `UPerceptionMeterComponent` |
| `OnChatMessageReceived` | `AShelterPlayerController` |
| `OnLobbyPlayerCountChanged` | `AShelterGameState` |

**클라이언트에서 서버 전용 데이터를 읽지 않도록 주의할 것.**
예: `UAlertComponent::GetNoiseContribution()` 은 클라에서 항상 비어 있다.
결과 화면은 `GetNoisiestPlayer()` 를 쓴다.

---

## 6. 다른 시스템과의 접점

| 상대 | 방향 | 수단 |
|---|---|---|
| 소음 · 경계도 (지성인) | ← 이쪽 | Dynamic 델리게이트 구독 |
| 세션 (이지은) | ← 이쪽 | 채팅 · 로비 인원 |
| 플레이어 · GAS (전영배) | ← 이쪽 | 스킬 쿨다운, 체력 · 스태미나 |

---

## 7. 디버깅

| 수단 | 비고 |
|---|---|
| `LogHeavyUI` | UI 전용 로그 카테고리 |
| `WBP_UITest` | `Content/Developers/Ji/` |

---

## 8. 알려진 제약과 TODO

> **TODO: 담당자가 정해져 있지 않다.** `feature/uiux` 브랜치의 커밋은 지성인이 올렸으나
> [06-Collaboration.md](../06-Collaboration.md) 2장 배정표에 UI 담당 행이 없다.
> 정식 담당을 정하고 배정표와 이 문서 머리말을 같이 고칠 것.

> **TODO:** 기획서 8장 중 미착수 —
> 소음 피드백(화면 가장자리 방향 표시), 상호작용 프롬프트, 경보 연동형 카운트다운,
> 준비 시간 UI, 은신처 상점, 결과 화면.

> **TODO:** CommonUI 는 도입하지 않기로 했다. 게임패드 · 포커스 관리가 실제 문제가 되면
> 재검토한다 ([08-Plugins.md](../08-Plugins.md) 3장).
