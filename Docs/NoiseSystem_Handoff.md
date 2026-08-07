# 소음 & 경계도 시스템 — 작업 인수인계

담당: 김지성 · 브랜치 `feature/NoiseAndBoundary` · 최종 갱신 2026-08-06

---

## 1. 지금 어디까지 왔나

| 단계 | 내용 | 상태 |
|---|---|---|
| 1 | 콜리전 채널 / 물리 재질 / 게임플레이 태그 / Build.cs | ✅ 완료 — `c9c3e7c` (develop 머지됨) |
| 2 | `NoiseTypes.h` 타입 정의 | ⚠️ **작성했으나 컴파일 실패. 내일 첫 할 일** |
| 3 | `NoiseSettings` (UDeveloperSettings) | ⬜ |
| 4 | `NoiseSubsystem` — 전파 · 감쇄 | ⬜ |
| 5 | `PerceptionMeterComponent` — 경비 인지 게이지 | ⬜ |
| 6 | `AlertComponent` — 경계도 4단계 | ⬜ |
| 7 | `NoiseEmitterComponent` — 물리 충돌 산출 | ⬜ |
| 8 | 지속형 소음 + 디버그 시각화 | ⬜ |

**1단계에서 확정된 것 (이미 커밋됨)**

- 트레이스 채널 `ECC_GameTraceChannel1` = `NoiseOcclusion`, 기본 응답 **Ignore**
- 물리 재질 `SurfaceType1~7` = Concrete / Wood / Metal / Glass / Carpet / Tile / Dirt
- 게임플레이 태그 9종 (`Config/DefaultGameplayTags.ini`)
- 모듈 의존: `GameplayTags`, `PhysicsCore`, `AIModule`, `DeveloperSettings`

---

## 2. 내일 첫 할 일 — `NoiseTypes.h` 고치기

### 무엇이 잘못됐나

빌드 로그(2026-08-06):

```
NoiseTypes.h(23): Error: Expected a GENERATED_BODY() at the start of the struct
NoiseTypes.h(34): Error: Expected a GENERATED_BODY() at the start of the struct
NoiseTypes.h(45): Error: Expected a GENERATED_BODY() at the start of the struct
```

| # | 문제 | 왜 문제인가 |
|---|---|---|
| 1 | `GENERATED_BODY()` 누락 | UHT 에러 |
| 2 | `NoiseTypes.generated.h` include 누락 | 반드시 **마지막** include여야 함 |
| 3 | `GameplayTagContainer.h` 누락 | `FGameplayTag` 미정의 |
| 4 | `Engine/DataTable.h` 누락 | `FTableRowBase` 미정의 |
| 5 | `UCurveFloat` 전방 선언 누락 | `TSoftObjectPtr`는 전방 선언으로 충분 |
| 6 | **`UPROPERTY()` 전무** | 컴파일러가 안 잡아줌. DataTable이 직렬화를 안 해서 **모든 행이 0으로 로드된다.** 소음이 안 나는 걸 며칠 헤매게 되는 버그 |
| 7 | `TWeakObjectPtr`를 BP 노출 시도 | BP 노출 불가 타입 |
| 8 | float 초기화 없음 | 미정의 값 |
| 9 | UTF-8 BOM 없음 + 한글 주석 | MSVC가 CP949로 읽으면 주석이 개행을 삼켜 다음 줄이 통째로 주석 처리됨 |

> **저장할 때 반드시 UTF-8 (BOM 포함)으로.** Visual Studio: 파일 → 다른 이름으로 저장 → 저장 버튼 옆 화살표 → 인코딩을 지정하여 저장 → "유니코드(서명 있는 UTF-8) - 코드 페이지 65001"

### 고친 전체 코드

`Source/HeavyHanded/Public/Noise/NoiseTypes.h` 를 아래로 통째 교체:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "NoiseTypes.generated.h"   // 반드시 마지막 include

class UCurveFloat;

/** 소음 등급. 계산은 연속값(Loudness01)으로 하고, 등급은 UI 표시와 AI 힌트에만 쓴다. */
UENUM(BlueprintType)
enum class ENoiseGrade : uint8
{
    None,
    Small,
    Medium,
    Large,
    Huge
};

/** 경계도 4단계. 기획서 3장. */
UENUM(BlueprintType)
enum class EAlertStage : uint8
{
    Calm,        // 평온   0~33%
    Suspicious,  // 의심  34~66%
    Alerted,     // 경계  67~99%
    Alarm        // 경보    100%
};

/** DT_NoiseProfiles 한 행. RowName == 게임플레이 태그 이름 (예: Noise.Loot.Throw) */
USTRUCT(BlueprintType)
struct FNoiseProfileRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 등급 라벨. UI와 AI 힌트 전용 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
    ENoiseGrade Grade = ENoiseGrade::Small;

    /** 경계도 증가량. 0.01 == 1% */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", UIMax = "0.5"))
    float AlertDelta = 0.01f;

    /** 전파 반경. 소 800 / 중 1500 / 대 3000 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", Units = "cm"))
    float Radius = 800.f;

    /** true면 전 구역. 거리 감쇄와 차폐를 모두 무시한다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
    bool bGlobal = false;

    /** 이 충격량 이하는 소음 없음 (물리 충돌 전용) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Impact", meta = (ClampMin = "0.0"))
    float MinImpulse = 0.f;

    /** 이 충격량 이상은 Loudness 1.0으로 포화 (물리 충돌 전용) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Impact", meta = (ClampMin = "0.0"))
    float MaxImpulse = 1000.f;

    /** 정규화된 충격량(0~1)을 Loudness로 매핑. 비워두면 선형 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise|Impact")
    TSoftObjectPtr<UCurveFloat> ImpactCurve;

    /** 같은 (에미터, 태그) 조합의 재발행 최소 간격. 충돌 스팸 방지 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise", meta = (ClampMin = "0.0", Units = "s"))
    float CooldownSeconds = 0.25f;
};

/**
 * 서버가 발행하는 소음 1건. 감쇄 적용 전.
 * 쿨다운 큐에 잠시 보관되므로 Instigator는 약참조로 둔다 (하드 레퍼런스로 잡으면
 * 파괴된 노획물이 GC되지 않는다). 그래서 BP에는 노출하지 않는다.
 */
USTRUCT(BlueprintType)
struct FNoiseEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FGameplayTag Tag;

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FVector Location = FVector::ZeroVector;

    /** 0~1. 충돌 속도 · 재질 · 모디파이어가 모두 반영된 연속값 */
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    float Loudness01 = 0.f;

    /** Loudness로 스케일된 실제 전파 반경 (cm) */
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    float Radius = 0.f;

    /** 소음을 낸 주체. 결과 화면 "최다 소음 유발자" 집계에 사용 */
    UPROPERTY()
    TWeakObjectPtr<AActor> InstigatorActor;
};

/**
 * 청취자 1명에게 전달되는 자극. 거리 · 차폐 감쇄 적용 완료.
 * 즉시 소비되고 버려지므로 하드 레퍼런스로 둬서 BP에 노출한다.
 */
USTRUCT(BlueprintType)
struct FNoiseStimulus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FGameplayTag Tag;

    /** 청취자가 조사하러 갈 지점 */
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    FVector Location = FVector::ZeroVector;

    /** 0~1. 감쇄까지 끝난 최종 강도. 인지 게이지 증가율에 그대로 곱한다 */
    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    float Strength = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    ENoiseGrade Grade = ENoiseGrade::None;

    UPROPERTY(BlueprintReadOnly, Category = "Noise")
    TObjectPtr<AActor> InstigatorActor = nullptr;
};
```

### `NoiseTypes.cpp` 처리

지금 0바이트입니다. 아래 한 줄만 넣으세요. 헤더가 자기 완결적인지(include 빠진 게 없는지) 검증해주는 역할입니다.

```cpp
#include "Noise/NoiseTypes.h"
```

### 검증

```powershell
& "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" `
  HeavyHandedEditor Win64 Development `
  -Project="D:\StudyProject\Unreal\HeavyHanded\HeavyHanded.uproject" -WaitMutex -NoHotReload
```

에러 0이면 통과. 통과 후 에디터를 한 번 띄워서 1단계 결과도 같이 확인:

- Project Settings → **Collision** → Trace Channels에 `NoiseOcclusion` (Default Response: Ignore)
- Project Settings → **Physics** → Physical Surface에 SurfaceType1~7
- Project Settings → **GameplayTags** → `Noise.*` 9개

---

## 3. 확정된 설계 결정 (다시 논의하지 말 것)

| 결정 | 선택 | 이유 |
|---|---|---|
| 경계도 게이지 소유 | **내 스코프에 포함** | 기획서 3장이 "소음과 경계도" 한 세트. 소음 만든 사람이 소비처까지 들고 있어야 밸런싱이 한 곳에서 됨 |
| 경비 인지 게이지 | **내가 컴포넌트만 제공** | 게이지 누적 계산은 나, 100% 도달 후 조사 행동/BT는 유정석 |
| 감쇄 방식 | **라인트레이스 오클루전** | NavMesh 경로거리는 비용 크고 문 개폐 반영이 까다로움 |
| 경계도 복제 위치 | **GameState에 붙이는 컴포넌트** | 이지은의 GameState 클래스 파일을 안 건드려서 머지 충돌 회피 |
| 소음 권위 | **서버 100%** | 클라 물리 충돌은 전부 무시 |

---

## 4. 남은 단계 상세

### 3단계 — `NoiseSettings` (UDeveloperSettings)

`Public/Noise/NoiseSettings.h`. DataTable 3개 경로 + 디버그 기본값을 프로젝트 세팅에 노출.
DataTable은 아직 없어도 되고 경로만 잡아두면 됨.

- `DT_NoiseProfiles` — 행동별 등급/경계도/반경
- `DT_SurfaceNoise` — 재질(EPhysicalSurface) → 계수
- `DT_NoiseOcclusion` — 차폐물 재질 → 감쇄율

### 4단계 — `NoiseSubsystem` (UWorldSubsystem)

작업의 본체. 여기까지 되면 나머지 4명에게 API를 공유할 수 있음.

```cpp
void ReportNoise(FGameplayTag Tag, FVector Loc, float LoudnessScale = 1.f, AActor* Instigator = nullptr);
FGuid StartContinuousNoise(FGameplayTag Tag, AActor* Source);
void  StopContinuousNoise(FGuid Handle);
void  RegisterListener(UObject* Listener);
void  UnregisterListener(UObject* Listener);
```

맨 앞에 `if (!GetWorld()->GetAuthGameMode()) return;` 같은 서버 가드 필수.

**감쇄 계산**

```
if (Dist > Radius) → 안 들림
Att = 1 - pow(Dist / Radius, 1.7)                       // 거리 감쇄
LineTraceMultiByChannel(NoiseOcclusion, 소음원 → 청취자)
for each blocker (최대 4개까지만):
    Att *= BlockerFactor(blocker의 PhysMat)              // 콘크리트 0.25 / 나무 0.45 / 문 0.6 / 유리 0.8
if (Loudness01 * Att < 0.05) → 안 들림
Stimulus.Strength = Loudness01 * Att
```

- `bTraceComplex = false`. 소음원 액터와 청취자 액터는 Ignore.
- **반경 안에 있는 청취자에 대해서만** 트레이스. 경비 10명 × 초당 5회 = 50 trace/s로 무시 가능한 비용.
- `bGlobal == true`면 트레이스 스킵, `Strength = 1.0` 고정.

### 5단계 — `PerceptionMeterComponent`

`BeginPlay`에서 서브시스템에 자기를 청취자로 등록. 자극을 받으면 게이지 누적, 무자극 시 감소.
100% 도달 시 `OnPerceptionFull(FVector LastNoiseLocation)` 브로드캐스트 → 유정석이 BT에서 구독.

### 6단계 — `AlertComponent`

GameMode의 `InitGameState()`에서 GameState에 런타임 부착. **`SetIsReplicated(true)` 필수.**

```cpp
AlertComp = NewObject<UAlertComponent>(GameState);
AlertComp->RegisterComponent();
```

> 이지은에게 "GameMode에 이 두 줄이 들어간다"고 미리 말해둘 것.

**경계도 상태 표 (기획서 모순 해소한 확정 스펙 — 기획자 확인 필요)**

| 단계 | 진입 | 이탈 (히스테리시스) | 감소 조건 | 감소율 |
|---|---|---|---|---|
| 평온 | 0% | → 34% | 30초 무소음 | -1%/초 |
| 의심 | 34% | ← 30% / → 67% | 30초 무소음 | -1%/초 |
| 경계 | 67% | ← 63% / → 100% | 60초 무소음 | -1%/초 |
| 경보 | 100% | 없음 (래치) | — | — |

- **히스테리시스 4%p 반드시 넣을 것.** 33/34% 경계에서 단계가 깜빡이면 셔터가 열렸다 닫혔다 함.
- 소음 발생 시 `SilenceTimer = 0` 리셋.

### 7단계 — `NoiseEmitterComponent`

`OnComponentHit` → 충격량 산출 → 스팸 필터 → 서브시스템 호출.

**충돌 소음 공식**

```
Impulse    = NormalImpulse.Size()        // 속도 아닌 충격량. 질량이 이미 반영됨
Base01     = Clamp((Impulse - MinImpulse) / (MaxImpulse - MinImpulse), 0, 1)
Shaped     = ImpactCurve->GetFloatValue(Base01)
Surface    = Min(SurfaceCoeff(내 PhysMat), SurfaceCoeff(상대 PhysMat))
Loudness01 = Clamp(Shaped * Surface * ModifierProduct, 0, 1)

Radius     = Row.Radius     * Lerp(0.6, 1.0, Loudness01)
AlertDelta = Row.AlertDelta * Loudness01
```

두 재질은 **`Min`(작은 쪽 = 흡음이 이김)**. 카펫 위에 떨어진 금고는 조용해야 직관적임.

**스팸 방지 — 반드시 넣을 것.** 와인 랙(불안정형)이 구르면 `OnComponentHit`이 초당 수십 번 온다. 그대로 더하면 한 번 굴린 걸로 경보 100%가 됨.

```
키 = (EmitterComponent, Tag)
1) 첫 충돌 → 즉시 발행, 쿨다운(CooldownSeconds) 시작
2) 쿨다운 중 추가 충돌 → 발행 안 함, PendingMax만 갱신
3) 쿨다운 종료 시 PendingMax > 이미 발행한 값이면 차액만 발행
4) 연속 충돌 카운트에 따라 체감 계수 (0.7^n, 하한 0.2). 2초 무충돌 시 리셋
```

**모디파이어는 인터페이스 말고 구조체로.** 전영배가 내 코드를 상속·수정하지 않게 하는 것이 목적.

```cpp
USTRUCT(BlueprintType)
struct FNoiseModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    FGameplayTagQuery AffectedTags;   // 예: Noise.Player 하위 전부

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    float Multiplier = 1.f;           // 고무창 0.5, 완충장갑 0.3

    /** 미믹 "발소리 위장" 처럼 배율이 아니라 태그를 갈아끼우는 경우 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
    FGameplayTag OverrideTag;
};
// UNoiseEmitterComponent::AddModifier(FNoiseModifier) -> FGuid
// UNoiseEmitterComponent::RemoveModifier(FGuid)
```

### 8단계 — 지속형 소음 + 디버그

- 지속형(대형 금고 절단 +3%/초): 4Hz 티커로 스케일된 이벤트 발행
- CVar `hh.Noise.Debug 0/1/2` — 1=소음 구체, 2=+감쇄 트레이스 라인
- CVar `hh.Alert.Set <pct>` — 경계도 강제 설정 (치트)
- 콘솔 명령 `hh.Noise.Test` — 임의 소음 발생. 4단계 검증에 필요하니 4단계에서 미리 만들 것

---

## 5. 팀 계약 — 다른 사람에게 공유할 API

4단계 끝나면 이 내용을 `Docs/NoiseAPI.md`로 따로 빼서 공지할 것.
**원칙: 다른 사람은 내 파일을 수정하지 않고 호출만 한다. 나도 다른 사람 클래스를 수정하지 않고 컴포넌트를 준다.**

```cpp
// [전영배/김민준/오유석] 소음 발생 — 서버에서만 유효
UNoiseSubsystem::Get(World)->ReportNoise(Tag, Location, 1.0f, InstigatorActor);

// [김민준] 지속 소음 (대형 금고 절단)
FGuid H = Sub->StartContinuousNoise(Noise_Device_SafeCut, Actor);
Sub->StopContinuousNoise(H);

// [전영배] 장비/패시브 소음 감쇄
FGuid M = EmitterComp->AddModifier({Query: Noise.Player, Multiplier: 0.5f});  // 고무창 신발

// [오유석/유정석] 경계도 단계 전환 구독 (셔터 폐쇄 / 경비 증원)
GameState->FindComponentByClass<UAlertComponent>()
         ->OnAlertStageChanged.AddDynamic(this, &AMyShutter::HandleStage);

// [유정석] 경비 BP에 UPerceptionMeterComponent 부착 후
PerceptionMeter->OnPerceptionFull.AddDynamic(this, &AGuard::StartInvestigate);
```

| 팀원 | 담당 | 나와의 접점 |
|---|---|---|
| 이지은 | 세션 & 네트워크 | GameMode에 AlertComponent 부착 2줄 |
| 전영배 | 플레이어 조작, 스킬 & 패시브 | `ReportNoise` 호출, `AddModifier` 등록 |
| 오유석 | 환경 방해 요소, 협력 장치 | `ReportNoise` 호출, `OnAlertStageChanged` 구독 |
| 유정석 | AI | `UPerceptionMeterComponent` 부착, `OnPerceptionFull` 구독 |
| 김민준 | 물리, 아이템 | `UNoiseEmitterComponent` 부착, 파괴 시 `ReportNoise` |

---

## 6. 아직 내가 결정 못 한 것

### (a) 타입 네이밍 프리픽스 — **내일 아침에 결정할 것**

지금 `ENoiseGrade` / `FNoiseEvent` 처럼 프리픽스가 없다. 원래 설계안은 `EHHNoiseGrade` / `FHHNoiseEvent` 였다.

- 프리픽스 없음: 짧고 읽기 편함. 다만 유정석이 AI 쪽에 `FNoiseEvent`를 따로 만들면 충돌
- `HH` 프리픽스: 5명이 타입을 쏟아낼 프로젝트에서 안전

**늦게 바꾸면 5명의 include가 전부 깨지므로 지금 정해야 한다.** 팀 컨벤션을 먼저 물어보고, 없으면 프리픽스를 붙이는 쪽을 권함.

### (b) GameTraceChannel 번호 배분 — **팀에 확인 요청 상태**

```
GameTraceChannel1 = NoiseOcclusion   (김지성)  ← 커밋 완료
GameTraceChannel2 = ?                (유정석 — 경비 시야?)
GameTraceChannel3 = ?                (오유석 — 레이저/센서?)
GameTraceChannel4 = ?                (김민준 — 운반 판정?)
```

각자 알아서 잡으면 같은 번호를 두 명이 써서 아무도 원인을 못 찾는 버그가 난다.

### (c) 기획서 수치 모순 — **기획자 확인 필요**

| 파라미터 | 3장 본문 | 부록 밸런싱 시트 | 내 판단 |
|---|---|---|---|
| 물건 충돌 | +1% | **+10%** | 3장(+1%) 채택. +10%면 10번 부딪혀 경보 |
| 낙하 | 물건 떨어뜨리기 +5% | 낙하 +5% | 플레이어 낙하와 물건 낙하 구분 필요 |
| 경계도 자연 감소 | "30초 무소음 시 하락" | -1%/초 | 부록 채택 |

그 외 정의 안 된 것:

- 소음이 동시에 여러 개 터질 때 합산 규칙
- "고무창 신발 이동 소음 -50%"인데 걷기는 원래 0% → 뛰기에만 적용되는 것인지
- 경계 단계에서 60초 못 채우고 소음이 나면 타이머 리셋인지 누적인지

**확정되면 `DT_NoiseProfiles`가 유일한 진리원이 된다. C++/BP에 수치 하드코딩 금지.**

### (d) 놓치지 말 것 — "최다 소음 유발자"

기획서 8장에 반드시 넣으라고 강조된 항목. `ReportNoise`에 이미 Instigator가 있으니
`TMap<APlayerState*, float>` 누적 한 줄이면 끝난다. 나중에 붙이려면 소음 이벤트를 다시 파고들어야 하므로
**6단계 AlertComponent 만들 때 같이 넣을 것.**

---

## 7. 자주 쓰는 명령어

```powershell
# 빌드
& "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" `
  HeavyHandedEditor Win64 Development `
  -Project="D:\StudyProject\Unreal\HeavyHanded\HeavyHanded.uproject" -WaitMutex -NoHotReload

# develop 최신화 후 내 브랜치에 반영
git checkout develop; git pull origin develop
git checkout feature/NoiseAndBoundary; git merge develop

# 커밋 전 실제로 올라갈 내용 확인
git --no-pager diff --cached
```

**환경 메모**

- UE 5.4 / `C:\Program Files\Epic Games\UE_5.4`
- MSVC 14.44 사용 중. UE 5.4 권장은 14.38이라 매 빌드마다
  `Visual Studio 2022 compiler version 14.44.35227 is not a preferred version` 경고가 뜬다. **무시해도 됨**
- `.gitattributes`가 `*.h *.cpp *.ini` 를 `text eol=lf`로 강제. 파일 끝 개행 넣을 것
- **한글 주석이 있는 소스는 UTF-8 with BOM으로 저장** (BOM 없으면 MSVC가 CP949로 읽어 주석이 개행을 삼킴)
