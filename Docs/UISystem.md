# UI / UX 시스템

담당: 김지성 · 브랜치 `feature/uiux`
기획서 8장(UI) 기준. 화면별 요소는 2장(코어 루프) · 4장(플레이어/역할) · 7장(장비)에서 끌어왔다.

**원칙: 다른 사람은 내 파일을 수정하지 않고 호출만 한다. 나도 다른 사람 클래스를 수정하지 않는다.**
UI는 데이터를 만들지 않고 **읽기만 한다.** 필요한 값은 담당자에게 게터·델리게이트를 요청한다 (5장).

---

## 1. 화면 목록

`Config/Tags/Phase.ini` 의 페이즈 태그와 1:1로 맞춘다.

| 페이즈 태그 | 화면 | 기획서 | 시안 |
|---|---|---|---|
| — | 타이틀 / 메인 메뉴 | (없음) | `ui_title.png` |
| `Phase.Lobby` | 로비 — 호스트/참가, 4인 슬롯, 준비 상태 | 8장 | **없음** |
| `Phase.Hideout` | 은신처 — 상점 · 스킬 강화 · 팀원 구출 · 목표 선택 | 2장, 8장 | `ui_shop.png` (상점 탭만) |
| `Phase.Prep` | 준비 45초 — 역할 선택, 장비 구매, 드론 뷰 | 2장, 8장 | **없음** |
| `Phase.Heist` | 인게임 HUD | 8장 | `ui_ingame.png` |
| `Phase.Escape` | 경보 90초 / 밴 탑승 (HUD 상태 전환) | 3장 | **없음** |
| `Phase.Result` | 결과 — 정산 · 기여도 · 최다 소음 유발자 | 8장 | `ui_result.png` |
| — | 일시정지 | (없음) | `ui_menu.png` |

시안 5장 중 기획서 화면과 대응하는 건 3장(은신처·HUD·결과)이고, **로비 · 준비 · 탈출 3개 화면은 시안이 없다.**

---

## 2. 디자인 토큰

시안 5장에서 픽셀 실측한 값이다. 시안이 압축 스크린샷이라 근사값이며, **골드만 4장에서 hue 41로 일치**해 확실하다.

| 토큰 | 값 | 근거 |
|---|---|---|
| `Bg/Base` | `#14181F` | 5장 전체 최빈색 |
| `Bg/Panel` | `#1A1E28` | `ui_result` 패널 실측 |
| `Bg/Card` | `#20242E` | `ui_shop` 카드 실측 (채도 보정) |
| `Line/Divider` | `#2C3140` | **제안값** — 측정 안 됨 |
| `Accent/Gold` | `#DEA934` | **4장 공통 실측.** 선택 테두리, 주 버튼, 강조 수치 |
| `Accent/GoldBright` | `#E9AF31` | 타이틀 로고 전용 |
| `Text/Primary` | `#F2F4F8` | |
| `Text/Secondary` | `#8A93A3` | 라벨, 부제 |
| `State/Health` | `#80E080` | `ui_ingame` 체력바 실측 |
| `State/Money` | `#6FD08C` | 가격·정산 텍스트 (안티에일리어싱 심해 근사) |

**이 10개는 `UUISettings` 에 있다** (Project Settings → Game → UI → Palette). `EUIColorToken` 으로 꺼낸다:

```
Get UI Color (Token = Gold)      → FLinearColor
```

색 에셋을 `Content/` 에 두지 않은 이유는 **UMG 디자이너에 찍은 색은 그대로 구워지기 때문**이다.
토큰을 나중에 고쳐도 이미 만든 위젯은 따라오지 않는다. 위젯이 `PreConstruct` 에서 읽어 스스로 칠하면
디자이너 편집 중에도 보이고(PreConstruct 는 디자이너에서도 돈다) 런타임에도 같은 색이 나온다.
시안 원본이 오면 Project Settings 값만 고치면 전 화면이 따라온다.

### 타이포그래피

크기를 위젯마다 찍으면 화면별로 제각각이 된다. **세 단계 밖으로 나가지 않는다.**

| `EUIFontToken` | 크기 | 쓰는 곳 |
|---|---|---|
| `타이머` | 28 Bold | 남은 시간, 목표 금액 — 화면에서 제일 큰 수치 |
| `수치` | 18 Regular | 체력 %, 가격, 노획물 가치 |
| `라벨` | 12 Regular | 라벨, 부제 |

```
Get UI Font (Token = 수치)      → FSlateFontInfo
```

**폰트 프로퍼티(`TimerFont` 등)를 직접 읽지 않는다.** BP에 아예 노출하지 않았다.
Project Settings의 Font 슬롯은 비어 있는 게 기본값이고(게임 폰트 미정), 빈 `FSlateFontInfo` 를
그대로 위젯에 꽂으면 **Slate가 LastResort 폰트로 떨어져 한글이든 숫자든 글자가 전부 네모가 된다.**
`GetUIFont()` 가 그때 엔진 기본 폰트에 크기·굵기만 얹어 돌려준다.

게임 폰트가 정해지면 Project Settings → Typography 의 Font 슬롯 세 개에 꽂는다. 그 뒤로는 꽂은 폰트가 쓰인다.

> **생성자에서 폰트를 로드하지 말 것.** `UTextBlock` 이 하는 것처럼
> `ConstructorHelpers::FObjectFinder<UFont>(*UWidget::GetDefaultFontName())` 를 쓰면
> **게임 모듈에서는 조용히 실패한다** — `UDeveloperSettings` CDO는 모듈 로드 중에 만들어지고
> 그 시점에 `/Engine/EngineFonts/Roboto` 를 못 읽는다. 컴파일도 경고도 통과하고 로그도 안 남으며,
> 위젯을 열어야 네모로 나오는 걸 본다. `FCoreStyle::GetDefaultFontStyle()` 은 에셋 로드가 없어 안전하다.

### 경계도 4단계 색상 — 신규 제안

기획서 8장이 "경계도 게이지(4단계 색상)"를 요구하지만 **시안에 게이지 자체가 없어** 새로 잡았다.
체력 초록과 겹치지 않도록 평온을 무채로 두고, 의심에 기존 골드를 재사용해 팔레트 안에서 상승감을 만든다.

| `EAlertLevel` | 단계 | 색 | 연출 |
|---|---|---|---|
| `Calm` | 평온 | `#8A93A3` | 없음 |
| `Suspicious` | 의심 | `#DEA934` | 채워질 때 한 번 펄스 |
| `Alerted` | 경계 | `#E3762F` | 상시 느린 펄스 |
| `Alarm` | 경보 | `#D93A34` | 점멸 + 90초 카운트다운 |

색만으로 단계를 구분하지 않는다. 단계 이름 텍스트를 항상 같이 띄운다 (색각 이상 대응).

---

## 3. 화면별 정의

### 3-1. 인게임 HUD (`Phase.Heist`)

기획서 8장의 HUD · 플레이어 · 스킬 · 소음 피드백 · 상호작용 5행이 전부 이 화면이다.

| 영역 | 요소 | 데이터 출처 | 상태 |
|---|---|---|---|
| 상단 중앙 | 남은 시간 (7/8/9분) | GameState — **대기** | ⛔ |
| 상단 중앙 | 현재 / 목표 금액 | GameState — **대기** | ⛔ |
| **상단** | **경계도 게이지 + 4단계 이름** | `UAlertComponent` | ✅ **지금 가능** |
| 좌하단 | 4인 파티 — 체력, 상태 아이콘 | PlayerState — **대기** | ⛔ |
| 우하단 | 스태미나, 무게 게이지 | Character — **대기** | ⛔ |
| 하단 중앙 | 스킬 3슬롯 쿨다운 + 공통 소모품 1슬롯 | GAS — **대기** | ⛔ |
| 하단 | 소지 노획물 (이름·가치·무게) | Loot — **대기** | ⛔ |
| 화면 가장자리 | 소음 방향 표시 | 서버→클라 경로 **없음** (아래) | ⛔ |
| 조준 대상 | 상호작용 프롬프트, 2인 캐리 대기 | Interaction — **대기** | ⛔ |
| 월드 | 경보 연동형 60초 카운트다운, 절단 진행률 | Loot — **대기** | ⛔ |
| 월드 (경비 머리 위) | **경비 인지 게이지** | `UPerceptionMeterComponent` | ✅ 지금 가능 |

**소음 방향 표시는 클라이언트에서 동작할 수 없다.** `UNoiseSubsystem::OnNoiseReported` 는 일반 C++
멀티캐스트 델리게이트라 서버에서만 발화하고, `ReportNoise()` 는 클라에서 조용히 무시되므로
`INoiseListener` 전파도 서버에서만 돈다. 지금 만들면 **호스트 창에서만 보이고 나머지 3명은 아무것도 못 본다** —
기획서 8장의 "누가 어디서 냈는지 팀 전원이 인지"가 성립하지 않는다.

`UWorldSubsystem` 은 액터가 아니라 RPC를 쓸 수 없다. 이미 GameState에 붙어 복제되는 `UAlertComponent` 에
얹는 것이 자연스럽다 (담당: 김지성):

```cpp
UFUNCTION(NetMulticast, Unreliable)   // 연출용이라 한두 개 놓쳐도 된다
void Multicast_NoiseHeard(FVector_NetQuantize Location, float Loudness01, APlayerState* Instigator);

UPROPERTY(BlueprintAssignable, Category = "Alert")
FOnNoiseHeardForUI OnNoiseHeardForUI;
```

**모든 소음을 멀티캐스트하면 대역폭이 터진다.** 걷기 · 물건 충돌까지 초당 수십 건이다.
중 · 대 · 특대 등급만, 또는 `Loudness01` 임계값 이상만 보내는 필터가 함께 있어야 한다.

시안 HUD에 **경계도 게이지가 없다.** 기획서에서 이 게임의 3대 기둥 중 2개(소음 관리, 장르 전환)가 전부 경계도로 표현되므로 자리를 새로 잡아야 한다. 남은 시간 옆 상단 중앙을 제안한다.

시안의 우상단 "목표 체크리스트"는 기획서에 대응이 없다. 기획서의 목표는 **금액 단일 조건**(저택 $50,000 / 박물관 $120,000 / 은행 $250,000)이라 체크리스트가 아니라 금액 진행바가 맞다.

### 3-2. 은신처 (`Phase.Hideout`)

기획서 2장 기준 기능 5개. 시안은 이 중 **장비 구매 하나만** 그려져 있다.

| 탭 | 내용 | 시안 |
|---|---|---|
| 수익 정산 | 직전 작업 결과, 개인 기여도 | 없음 (결과 화면과 통합 가능) |
| 장비 구매 | 장비 8종, 런 단위 유지 | `ui_shop.png` ✅ |
| 스킬 강화 | 역할별 스킬 쿨다운·지속시간 수치 강화 | 없음 |
| 팀원 구출 | 체포된 팀원 비용 지불 복귀 | 없음 |
| 목표 선택 | 다음 장소 확인 및 출발 | 없음 |

장비는 `Config/Tags/Equipment.ini` 의 8개와 기획서 7장 표 8개가 정확히 일치한다.
(기획서 2장 본문의 "7장 장비 목록"은 **8종의 오타**로 보인다 — 확인 필요.)

| 태그 | 장비 | 가격 |
|---|---|---|
| `Equipment.RubberShoes` | 고무창 신발 | $8,000 |
| `Equipment.PaddedGloves` | 완충 장갑 | $12,000 |
| `Equipment.HandCart` | 대차 | $20,000 |
| `Equipment.EMP` | EMP 장치 | $10,000 |
| `Equipment.Decoy` | 미끼 | $6,000 |
| `Equipment.MedKit` | 응급 키트 | $7,000 |
| `Equipment.Drone` | 정찰용 드론 | $4,000 |
| `Equipment.Cutter` | 절단기 | $25,000 |

절단기($25,000)는 대형 금고 개방의 유일한 수단이고, 기획서가 "이번 판에 절단기를 살 것인가"를 은신처의 핵심 판단으로 지정했다. **상점 UI에서 절단기는 다른 장비와 같은 크기로 두지 않고 별도 강조**한다.

### 3-3. 준비 (`Phase.Prep`) — 시안 없음

45초 타이머 아래 역할 4종 선택. 4인이 동시에 고르므로 **다른 사람이 뭘 골랐는지 실시간**으로 보여야 한다.
중복 선택 허용 여부는 기획서에 없다 — 결정 필요 (7장).

| 역할 | 액티브 A | 액티브 B | 팀 시너지 |
|---|---|---|---|
| `Role.Brute` 브루트 | 벽 뚫기 돌진 | 동료 투척 | 원맨 캐리 |
| `Role.Ghost` 고스트 | 그림자 이동 | 시간 늦추기 | 그래플 라인 |
| `Role.Oracle` 오라클 | 시야각 투영 | 소원의 나침반 | 전 구역 스캔 |
| `Role.Mimic` 미믹 | 경비 변장 | 바디 스왑 | 카메라 루프 |

드론 보유 시 이 시간에 드론 탐색 뷰로 전환된다.

### 3-4. 결과 (`Phase.Result`)

| 요소 | 시안 | 비고 |
|---|---|---|
| 성공 / 실패 | 성공만 | **실패 레이아웃 필요** (목표 금액 미달 / 전멸) |
| 적재 목록 | ✅ 획득 현상 | |
| 획득 금액 | ✅ | |
| 개인 기여도 | ✅ 플레이어별 금액 | |
| **최다 소음 유발자** | **없음** | 기획서가 "반드시 넣는다"고 명시. `GetNoisiestPlayer()` 로 **지금 가능** |
| 재도전 | 없음 | 실패 시 같은 장소 재시작 |
| 체포자 표시 | 없음 | 미승차 인원 = 노획물 몰수 |

기획서 8장 주석: *"최다 소음 유발자 표시는 반드시 넣는다. 결과 화면에서 책임 소재가 드러나는 것이 이 게임의 코미디를 완성한다."*
→ 부가 정보가 아니라 **결과 화면의 주인공**으로 배치한다.

---

## 4. 지금 붙일 수 있는 것

플레이어 · 노획물 · 장비 · 세션 클래스가 아직 하나도 없다 (`Source/HeavyHanded/` 에 `Noise/` 와 `Alert/` 만 존재).
**실데이터가 있는 건 소음/경계도 계열 3개뿐**이고, 이게 전부 HUD와 결과 화면의 핵심이다.

```cpp
#include "Alert/AlertComponent.h"

// 경계도 게이지 — HUD 상단
if (UAlertComponent* Alert = UAlertComponent::Get(this))
{
    Alert->OnAlertGaugeChanged.AddDynamic(this, &UMyHUD::HandleGauge);   // float 0~1
    Alert->OnAlertLevelChanged.AddDynamic(this, &UMyHUD::HandleLevel);   // EAlertLevel 신·구
}

// 최다 소음 유발자 — 결과 화면
float Contribution = 0.f;
APlayerState* Noisiest = Alert->GetNoisiestPlayer(Contribution);
```

**단계는 게이지에서 직접 계산하지 않는다.** 히스테리시스가 있어 65%가 상승 중이면 의심, 하강 중이면 경계다.
반드시 `GetAlertLevel()` 을 쓴다. 자세한 건 [NoiseSystem.md](NoiseSystem.md) 3장.

경비 인지 게이지는 `UPerceptionMeterComponent::OnPerceptionChanged` 를 월드 스페이스 위젯에 물린다.

나머지 화면은 **목 데이터로 껍데기부터** 만든다. 5장의 인터페이스가 채워지는 대로 교체한다.

---

## 5. 다른 담당자에게 요청할 API

UI가 남의 클래스를 고치지 않으려면 읽을 창구가 필요하다. 담당자별로 아래를 요청한다.

| 담당 | 필요한 것 | 쓰는 화면 |
|---|---|---|
| 이지은 (network) | `Phase` 태그 현재값 + 변경 델리게이트, 남은 시간, 현재/목표 금액, 팀 소지금 | 전 화면 전환 |
| 이지은 (network) | 로비 세션 목록, 4인 슬롯 상태, 준비 완료 플래그 | 로비 |
| 전영배 (player) | 체력 · 스태미나 · 무게(0~1), `State.*` 태그 보유 여부, 선택 역할 | HUD, 준비 |
| 전영배 (skills) | 스킬 3슬롯 쿨다운 남은 시간(0~1), 소모품 잔량 | HUD |
| 김민준 (physics) | 소지 노획물 (이름·가치·무게·특성 태그), 경보 카운트다운 남은 초, 절단 진행률 | HUD |
| 김민준 (physics) | 2인 캐리 대기 상태, 상호작용 프롬프트 대상 | HUD |
| 오유석 (env) | — (경계도 구독만, 이미 있음) | — |
| 유정석 (AI) | 경비 BP `BeginPlay` 에서 `BindToGuard(Self)` **한 번** (6장 참조) | 경비 인지 게이지 |

**요청 형식: `BlueprintPure` 게터 + `BlueprintAssignable` 델리게이트.** 폴링하지 않고 구독한다.
`UFUNCTION` 반환 타입에 `TObjectPtr` 를 쓰면 UHT 에러가 나므로 원시 포인터로 받는다 (CLAUDE.md 함정 1).

---

## 6. 위젯 규칙

- 접두사 `WBP_` (컨벤션 4-1). 예: `WBP_AlertGauge`, `WBP_ShopCard`, `WBP_ResultPanel`
- 폴더는 기존 스캐폴딩을 그대로 쓴다 — `Content/HeavyHanded/UI/{Common,HUD,Lobby,Hideout,Result,Icons,Fonts}`
  - 준비 화면은 `Hideout/` 에 함께 둔다 (`Prep/` 신설하지 않음)
- **CommonUI는 쓰지 않는다.** `HeavyHanded.uproject` 는 6명 공유 파일이고 플러그인 추가는 팀 합의가 필요하다.
  화면 5개 규모에는 `UMG` + 얇은 C++ 베이스로 충분하다. 게임패드 지원이 요구사항이 되면 그때 재검토한다.
- C++ 베이스는 `Source/HeavyHanded/{Public,Private}/UI/` 에 두고, **레이아웃·애니메이션은 BP에서** 한다.
  C++에는 데이터 바인딩과 구독 해제만 넣는다.
- 델리게이트 구독은 `NativeConstruct`, 해제는 `NativeDestruct`. **해제를 빠뜨리면 위젯이 GC되고도 콜백이 남는다.**
- **보여주기만 하는 위젯은 루트 `Visibility` 를 `Not Hit-Testable (Self & All Children)` 로 둔다.**
  기본값 `Self Only` 는 자기 자신만 클릭을 안 받고 자식은 그대로 받는다. `Border` · `ProgressBar` 는
  기본이 hit-testable 이라 HUD가 그 영역의 마우스 입력을 삼킨다. 버튼처럼 입력을 받아야 하는 것만 예외다.

### 만들어진 C++ 베이스

| 클래스 | 위치 | 상속할 WBP |
|---|---|---|
| `UUISettings` | `UI/UISettings.h` | — (Project Settings → Game → UI) |
| `UAlertGaugeWidget` | `UI/AlertGaugeWidget.h` | `WBP_AlertGauge` |
| `UPerceptionMeterWidget` | `UI/PerceptionMeterWidget.h` | `WBP_PerceptionMeter` |

**색과 폰트는 `UUISettings` 하나만 본다.** 위젯 BP에 색을 직접 찍지 말 것 —
HUD · 결과 화면 · 상점이 각자 다른 색을 쓰게 된다.

| 무엇 | 어떻게 |
|---|---|
| 토큰 색 | `Get UI Color (Token)` — static 이라 노드 하나 |
| 폰트 | `Get UI Font (Token)` — 프로퍼티 직접 읽기 금지 (2장) |
| 경계도 4단계 색 | `OnLevelUpdated` 의 `LevelColor` 인자 (직접 조회하지 않는다) |
| 경계도 단계 이름 | `Get Alert Level Text (Level)` |

토큰을 적용하는 자리는 **`Event PreConstruct`** 다. `Construct` 가 아니다 —
`PreConstruct` 는 디자이너에서도 실행돼서 편집 중에도 실제 색으로 보인다.
`Construct` 에 넣으면 디자이너에는 흰 상자만 보이고 실행해야 색이 나온다.

`UAlertGaugeWidget` 은 `UAlertComponent` 를 찾을 때까지 0.25초 간격으로 재시도한다.
컴포넌트가 GameState에 **런타임 부착 후 복제**로 도착하기 때문에, 한 번 찾고 포기하면
호스트 창은 멀쩡한데 클라이언트에서만 게이지가 0으로 고정된다.

`UPerceptionMeterWidget` 은 **소유 경비를 스스로 알 수 없다.**
`UWidgetComponent::InitWidget()` 이 `CreateWidget(World, ...)` 으로 만들어서 Outer가 World이고
액터로 거슬러 올라갈 방법이 없다. 경비 BP에서 한 번 연결해야 한다:

```
Event BeginPlay
  → WidgetComponent → Get User Widget Object
  → Cast To PerceptionMeterWidget
  → Bind To Guard (Self)
```

안 부르면 2초 뒤 `LogHeavyUI` 에 경고가 남는다 (조용히 실패하지 않게).

---

## 7. 결정 필요

### 기획자 확인

| 항목 | 문제 |
|---|---|
| **게임 이름** | 기획서·컨벤션은 `Sneakers`, 시안 로고는 `SCRAP & ESCAPE`. 타이틀 화면이 막힌다 |
| **상점 "무기" 카테고리** | 시안 `ui_shop` 에 무기 탭이 있으나 기획서 1장은 *"총이 없다. 싸울 수 없다"*. 카테고리 재정의 필요 |
| 장비 개수 | 기획서 2장 "7장 장비 목록" vs 7장 표 8종 + 태그 8종. 8종이 맞는가 |
| 역할 중복 선택 | 4인이 같은 역할을 고를 수 있는가. 준비 화면 UI가 갈린다 |
| HUD 미니맵 | 시안에 상시 미니맵이 있으나 기획서는 오라클 팀 시너지("미니맵에 10초간 표시")에서만 언급. 상시인가 |
| 결과 등급 | 시안에 A 등급 표기. 기획서에 산정식 없음 |
| 경보 연동형 지연 | 기획서 5장 "60초 카운트다운"인데 지연 설명은 "30초 → 90초". 기준값이 60인지 30인지 |

### 팀 확인

- **로비 화면은 이지은(network)과 겹친다.** 세션 로직은 network, 위젯은 uiux로 나눌지 합의 필요
- 결과 화면과 은신처 "수익 정산"이 같은 내용이다. 하나로 합칠지

---

## 8. 작업 순서

실데이터가 있는 것부터 간다. 목 데이터 화면을 먼저 만들면 남의 API가 확정될 때 전부 다시 짜야 한다.

| # | 작업 | 상태 |
|---|---|---|
| 1 | `UUISettings` · `UAlertGaugeWidget` · `UPerceptionMeterWidget` C++ 베이스 | ✅ 빌드 완료 |
| 2 | `WBP_AlertGauge` — 경계도 4단계 게이지 | ✅ 색 전환 · 경보 점멸 · **클라 복제** 확인 |
| 3 | `WBP_PerceptionMeter` — 경비 머리 위 게이지 | ✅ 게이지 상승 · **클라 복제** 확인 |
| 4 | 디자인 토큰 — 팔레트 10색 · 폰트 3단계 | ✅ `UUISettings` (2장) |
| 5 | 공통 위젯 — `WBP_Button` / `WBP_Card` / `WBP_StatBar` | ✅ `Common/`. 확인은 `Developers/Ji/WBP_UITest` |
| 6 | `WBP_HUD` 골격 + 목 데이터 (시간 · 금액 · 체력 · 스킬 슬롯) | 🔸 골격 · 앵커 · 게이지 이관 완료. 노획물 영역과 **2인 복제 검증** 남음 |
| 7 | 소음 방향 표시 — 화면 가장자리 인디케이터 | ⛔ 서버→클라 경로 선행 (3-1장) |
| 8 | 결과 화면 — 최다 소음 유발자 중심 | |
| 9 | 은신처 상점 — 장비 8종 | |
| 10 | 로비 · 준비 | 7장 결정 후 |

2~3번은 PIE **Number of Players 2 / Run Under One Process 해제 / Play As Listen Server** 로
클라이언트 복제까지 바로 검증한다. 체크를 해제해야 별도 프로세스로 떠서 진짜 네트워크 경로를 탄다.

경계도는 콘솔로도 확인된다 — `DisplayAll AlertComponent AlertGauge` (자세한 건 [NoiseSystem.md](NoiseSystem.md) 5장).

---

## 부록. WBP 레퍼런스 구현

C++ 베이스를 상속하는 첫 두 위젯이다. 이후 위젯도 같은 형태를 따른다 —
**C++ 이벤트를 받아 위젯 변수에 꽂기만 하고, 값 계산은 하지 않는다.**

### WBP_AlertGauge

`Content/HeavyHanded/UI/HUD/WBP_AlertGauge` · 부모 클래스 `AlertGaugeWidget`

```
SizeBox                        Width 320  Height 44
└ Overlay
   ├ Border            [BG]    Brush 흰색 (A 0.85) · Padding 3
   │   └ Overlay
   │      ├ Image        [Trough]   Fill / Fill · 브러시 흰색
   │      └ ProgressBar  [Bar]      Percent 0 · Background Image Tint A 0
   └ HorizontalBox             Padding 10, 0
       ├ TextBlock     [Txt Level]     Fill(1) · Left  · "평온"
       └ TextBlock     [Txt Percent]   Auto    · Right · "0%"
```

`BG` · `Trough` · `Bar` · `Txt Level` · `Txt Percent` 는 **Is Variable** 을 켠다.

**바 뒤에 `Image [Trough]` 를 따로 깐 이유** — `UProgressBar` 의 블루프린트 세터는 5.4 기준
`SetPercent` · `SetIsMarquee` · `SetFillColorAndOpacity` 셋뿐이다. **Background Image 의 Tint 는
BP 에서 바꿀 수 없다.** 디자이너에 색을 찍는 수밖에 없어 토큰 밖으로 새어나가므로,
바 배경을 투명(Tint A 0)으로 만들고 뒤에 깐 `Image` 가 트로프 색을 맡는다.

**교체냐 곱셈이냐가 위젯마다 다르다.** 여기서 색이 어긋나면 원인 찾기가 오래 걸린다.

| 노드 | 동작 | 디자이너 값 |
|---|---|---|
| `Border → Set Brush Color` | 브러시 틴트를 **교체** | 무시된다. 알파도 노드가 준 값이 된다 |
| `Image → Set Color and Opacity` | 브러시 틴트에 **곱셈** | **흰색으로 둬야** 토큰 색이 그대로 나온다 |
| `Button → Set Background Color` | 스타일 브러시에 **곱셈** | 브러시를 흰색으로 |

`Bg` 의 알파 0.85 는 디자이너가 아니라 `Set Brush Color` 에 넣는 값에 들어간다 (교체이므로).

**애니메이션 `AlarmBlink` 는 에디터에서 직접 만든다.** Designer 탭 → Animations 패널 → `+ Animation`.
`BG` 트랙에 Render Opacity 키를 `0.0s → 1.0` / `0.25s → 0.35` / `0.5s → 1.0` 로 찍는다 (길이 0.5초 = 2Hz).
위젯 블루프린트에는 Timeline 노드가 없어서 UMG 애니메이션 말고는 방법이 없다.

애니메이션 길이는 에셋에 고정이므로 **`Play Animation` 의 `Playback Speed` 에
`AlarmBlinkHz / 2.0` 을 넣어야** `UUISettings::AlarmBlinkHz` 가 죽은 값이 되지 않는다
(애니메이션이 2Hz로 작성돼 있으므로). `AlarmBlinkHz <= 0` 이면 배속 0으로 얼어붙으니
그때는 `Play Animation` 을 타지 않게 분기한다.

| 이벤트 | 하는 일 |
|---|---|
| `Event PreConstruct` | `BG → Set Brush Color` (`Get UI Color (패널)` → `Break`/`Make Linear Color` 로 **A 만 0.85**) · `Trough → Set Color and Opacity (Get UI Color (배경))` · `Txt Percent → Set Color and Opacity (Get UI Color (본문))` · 두 텍스트 `Set Font (Get UI Font (수치))` |
| `On Gauge Updated (NewGauge01)` | `Bar → Set Percent (NewGauge01)` · `Txt Percent → Set Text` (`NewGauge01 × 100` → `To Text (Float)` **Maximum Fractional Digits 1** → `Format Text "{Pct}%"`) — 밸런싱 중에는 소수 한 자리가 보이는 편이 낫다 |
| `On Level Updated (NewLevel, OldLevel, LevelColor)` | `Bar → Set Fill Color and Opacity (LevelColor)` · `Txt Level → Set Text (Get Alert Level Text (NewLevel))` · `Txt Level → Set Color and Opacity (LevelColor)` · `NewLevel == Alarm` 이면 `Play Animation (AlarmBlink, Num Loops 0, Playback Speed = AlarmBlinkHz / 2)`, 아니면 `Stop Animation` + `BG → Set Render Opacity (1.0)` |

바 색은 `Set Fill Color and Opacity`(`FLinearColor` 직접), 텍스트 색은 `Set Color and Opacity`(`Make SlateColor` 경유)다.
이름이 비슷해서 헷갈리기 쉬운데, **바 쪽을 빠뜨리면 텍스트 색만 바뀌고 게이지는 평온 회색에 고정된다.**

**`Txt Level` 의 색과 `Bar` 의 채우기 색은 `PreConstruct` 에서 건드리지 않는다.** 둘은 `OnLevelUpdated`
가 매번 덮어쓰는 값이라, 여기서 토큰 색을 칠해봐야 바인딩되는 순간 사라진다. 경계도 4단계는
팔레트가 아니라 `UUISettings` 의 별도 항목이고 창구는 `LevelColor` 인자 하나다.

`Set Percent` 에 들어가는 값은 0~1 그대로이고, 퍼센트 표시에만 ×100 을 한다.
`Stop Animation` 쪽에서 Render Opacity 를 1.0 으로 되돌리지 않으면
경보가 풀릴 때 애니메이션이 멈춘 순간의 투명도로 굳는다.

**색과 이름을 BP에 하드코딩하지 않는다.** 색은 `On Level Updated` 의 `LevelColor` 인자를,
이름은 `Get Alert Level Text` 노드를 쓴다. 둘 다 `UUISettings` / `EAlertLevel` 이 유일한 출처다.

최초 바인딩 때도 두 이벤트가 한 번씩 호출된다 (이때 `NewLevel == OldLevel`). 초기화 로직을 따로 두지 않아도 된다.

### WBP_PerceptionMeter

`Content/HeavyHanded/UI/HUD/WBP_PerceptionMeter` · 부모 클래스 `PerceptionMeterWidget`

```
SizeBox                        Width 64  Height 10
└ ProgressBar       [Meter]    Percent 0 · Fill #DEA934
```

| 이벤트 | 하는 일 |
|---|---|
| `On Perception Updated (NewPerception01)` | `Meter → Set Percent` |
| `On Meter Visibility Changed (bShouldShow)` | `Self → Set Visibility` (`Visible` / `Collapsed`) |

가로 바는 데이터 흐름 검증용 MVP다. 기획서 8장의 "원형 게이지"는
방사형 채우기 머티리얼(`Percent` 스칼라 파라미터)로 교체한다.

### 토큰 적용 — 공통 위젯

`Content/HeavyHanded/UI/Common/`. 부모 클래스는 `UserWidget` 그대로 둔다 (C++ 베이스 불필요 —
색을 칠하는 것 말고 하는 일이 없다).

| 위젯 | 구조 | `Event PreConstruct` |
|---|---|---|
| `WBP_Button` | `SizeBox` → `Button [Btn]` → `TextBlock [Label]` | `Btn → Set Background Color (Get UI Color (BgCard))` · `Label → Set Font (Get UI Font (수치))` · `Label → Set Color and Opacity (Get UI Color (TextPrimary))` |
| `WBP_Card` | `Border [Bg]` → `NamedSlot [Content]` | `Bg → Set Brush Color (Get UI Color (BgCard))` |
| `WBP_StatBar` | `SizeBox(H 18)` → `Overlay` → `Image [Trough]` + `ProgressBar [Bar]` + `TextBlock [Txt Label]` | `Trough ← 배경` · `Bar → Set Fill Color and Opacity ← Get UI Color (FillToken)` · `Txt Label ← 라벨 폰트 · 본문` |

`WBP_StatBar` 의 `FillToken` (`EUIColorToken`) 만 **Instance Editable + Expose on Spawn** 이다.
체력 · 스태미나 · 무게 · 파티 4인이 색만 바꿔 같은 위젯을 쓴다. **인스턴스 변수는 여기까지다** —
정렬이나 라벨 색까지 인스턴스로 빼면 배치한 곳마다 달라져서 토큰을 만든 의미가 없어진다.

**`PreConstruct` 에서 `FillToken` 을 `SET` 하지 말 것.** 인스턴스에서 지정한 값을 매번 덮어써서
전부 같은 색이 된다. 기본값은 노드가 아니라 변수 Details 의 Default Value 에 넣는다.

라벨이 채워지는 바 **위에** 얹히므로 색 하나로는 대비가 안 나온다 (바가 비면 어두운 트로프,
차면 밝은 채우기 색). 라벨은 `본문` 에 **Shadow Offset `1,1` · Shadow Color 검정 A 0.6** 을 준다.
HUD 텍스트가 배경 위에 얹히는 곳(`Txt_Timer`, `Txt_Interact`)도 같은 처리를 한다.

**`Label` 같은 표시용 텍스트는 `Is Variable` 을 켜야** PreConstruct에서 잡힌다.
`WBP_Button` 의 문구는 `Instance Editable` + `Expose on Spawn` 인 `FText` 변수로 받아
디자이너에서 배치할 때 바로 넣는다.

버튼 색은 `Set Style` 이 아니라 **`Set Background Color`** 다. `Set Style` 은 브러시 4개짜리
`FButtonStyle` 을 통째로 만들어야 해서 BP에서 짤 것이 못 된다. `Set Background Color` 는
스타일 브러시에 색을 곱하므로 **디자이너에서 브러시를 흰색으로 둬야** 토큰 색이 그대로 나온다.

호버는 같은 노드를 이벤트 두 개에 물린다 — `On Hovered` → `Gold`, `On Unhovered` → `BgCard`.
곱셈이라 상태별 브러시를 따로 두면 색이 겹쳐 어두워진다.

`WBP_AlertGauge` 에서 하드코딩 색을 걷어내는 방법은 이 문서 부록의 `WBP_AlertGauge` 항목에 있다.
`Bar` 의 Background Tint 만은 BP에서 못 바꿔서 구조를 한 겹 바꿔야 한다.

### WBP_HUD

`Content/HeavyHanded/UI/HUD/WBP_HUD` · 부모 클래스 `UserWidget` · 1920×1080 기준

```
Canvas Panel [Root]        Visibility = Not Hit-Testable (Self & All Children)
├ VBox_Top      ├ Txt_Timer · Bar_Objective(금액) · Gauge_Alert
├ VBox_Party    └ Bar_Party1~4 (체력)
├ VBox_Vitals   └ Bar_Stamina(골드) · Bar_Weight(금액)
├ VBox_Loot
├ HBox_Skills   └ Card_Skill1~4 (WBP_Card)
└ Txt_Interact
```

| 위젯 | Anchors (Min = Max) | Alignment | Position | Size |
|---|---|---|---|---|
| `VBox_Top` | 0.5, 0 | 0.5, 0 | 0, 24 | 520 × 140 |
| `VBox_Party` | 0, 1 | 0, 1 | 32, −32 | 280 × 116 |
| `VBox_Vitals` | 1, 1 | 1, 1 | −32, −32 | 260 × 52 |
| `VBox_Loot` | 1, 1 | 1, 1 | −32, −104 | 260 × 160 |
| `HBox_Skills` | 0.5, 1 | 0.5, 1 | 0, −32 | 376 × 88 |
| `Txt_Interact` | 0.5, 0.5 | 0.5, 0.5 | 0, 120 | Size To Content |

**Anchors 는 Minimum 과 Maximum 을 같은 값으로**, **Alignment 는 프리셋으로 안 잡히니 직접 입력**한다.
Alignment 를 빼먹으면 코너 기준이 아니라 좌상단 기준이 되어 화면 밖으로 나간다.

**박스 안의 `WBP_StatBar` 는 Horizontal Alignment 를 `Fill`, Size 를 `Auto` 로 둔다.**
`WBP_StatBar` 의 루트 `SizeBox` 는 Height 18 만 있고 Width 가 없어서, 가로 정렬이 `Center` 면
내용물 폭으로 쪼그라들어 **16픽셀짜리 사각형**이 된다. `Txt_Timer` 만 `Center` 다.
Size 가 `Fill` 이면 세로로 늘어나 박스의 남은 높이를 먹는다.

`Txt_Timer` 는 타이머 폰트, `Txt_Interact` 는 수치 폰트를 `PreConstruct` 에서 넣는다.

레벨에는 HUD 하나만 띄운다. 게이지는 HUD 의 자식이라 따로 만들지 않는다:

```
Event BeginPlay → Create Widget (WBP_HUD, Get Player Controller 0) → Add to Viewport
```

게이지를 독립으로 띄울 때 필요했던 `Set Anchors / Alignment / Position in Viewport` 3노드는
**HUD 에서는 필요 없다.** `Add to Viewport` 의 기본 스트레치 앵커가 전체 화면 HUD 에는 맞는 동작이다.

### 검증 — `L_NoiseTest`

경비 AI 없이도 지금 확인된다. `ANoiseTestListener` 에 `UPerceptionMeterComponent` 가 이미 붙어 있다.

**경계도** — 레벨 블루프린트 `BeginPlay` → `Create Widget (WBP_AlertGauge, Get Player Controller 0)` → `Add to Viewport`

```
→ Set Anchors in Viewport    (Minimum 0.5, 0  /  Maximum 0.5, 0)
→ Set Alignment in Viewport  (0.5, 0)
→ Set Position in Viewport   (0, 40)
```

**앵커 3노드를 빠뜨리면 위젯이 화면 전체로 늘어난다.** `Add to Viewport` 의 기본 슬롯 앵커가
`(0,0)~(1,1)` 스트레치라서 루트 SizeBox 의 크기 지정이 무시된다 — `GameViewportSubsystem.cpp` 의

```cpp
bool bUseAutoSize = FinalSize.IsZero()
    && !Slot.Anchors.IsStretchedVertical() && !Slot.Anchors.IsStretchedHorizontal();
```

Minimum 과 Maximum 을 **같은 값**으로 줘야 스트레치가 아니게 되어 위젯의 desired size 가 쓰인다.
`Set Desired Size in Viewport` 로도 되지만 크기 정의가 두 곳으로 갈라지므로 쓰지 않는다.

이 3노드는 `WBP_HUD`(Canvas Panel 루트) 가 생기면 사라진다. 게이지는 독립 화면이 아니라 HUD의 부품이다.

**인지 게이지** — `ANoiseTestListener` 를 상속한 `BP_NoiseTestListener` 를 만들고 `Widget` 컴포넌트 추가:

| 속성 | 값 |
|---|---|
| Widget Class | `WBP_PerceptionMeter` |
| Space | `Screen` |
| Draw Size | 64 × 10 |
| Location | Z +120 (머리 위) |

`BeginPlay` → `Widget → Get User Widget Object` → `Cast To PerceptionMeterWidget` → `Bind To Guard (Self)`

`Get User Widget Object` 가 null 이면 위젯 컴포넌트의 `Tick When Offscreen` 을 켜거나 한 틱 미루고 호출한다.

**만든 `BP_NoiseTestListener` 를 레벨에 직접 배치할 것.** 레벨에 이미 놓여 있는 것은 C++ `ANoiseTestListener` 라
위젯 컴포넌트가 없다. 기존 액터를 교체하지 않으면 게이지가 하나도 안 보인다.

`hh.Noise.Test` 는 **플레이어 위치에서** 소음을 내므로, 리스너를 플레이어 스타트 근처에 놔야 반응 반경 안에 든다.

**호스트 창 콘솔** ([NoiseSystem.md](NoiseSystem.md) 5장):

```
hh.Alert.Set 70              # 경계도 70% — 색이 경계(주황)로 바뀌는지
hh.Alert.Set 100             # 경보 — 점멸이 도는지
hh.Alert.Reset               # 래치 해제
hh.Noise.Test Noise.Loot.Throw 1.0    # 인지 게이지가 차오르는지
```

**클라이언트 창**에서 값이 따라오는지가 진짜 검증이다. 호스트만 보고 넘어가면 복제 누락을 놓친다.

```
obj list class=AlertComponent
DisplayAll AlertComponent AlertGauge
```
