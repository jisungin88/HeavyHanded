# 04. 데이터 구조 및 데이터 관리

> 이 문서는 **커밋되어 확인 가능한 코드와 설정**을 기준으로 쓴다.
> **수치 자체는 여기 적지 않는다.** 어디를 보면 되는지만 적는다 — 문서에 수치를 복사하면 반드시 어긋난다.

---

## 1. 이 문서의 범위

**여기서 정하는 것**

- 데이터를 어디에 두는가 (네 저장소와 선택 기준)
- GameplayTag 체계와 태그 추가 절차
- DataTable 규약
- Project Settings(`UDeveloperSettings`) 사용법
- 에셋을 어떻게 참조하는가

**여기서 정하지 않는 것**

| 주제 | 문서 |
|---|---|
| 에셋 접두사 · 폴더 구조 | [05-Conventions.md](05-Conventions.md) 9·10장 |
| `Config/Tags/*.ini` 담당자 배정 | [06-Collaboration.md](06-Collaboration.md) |
| 값을 BP 가 지정한다는 원칙 | [03-CppVsBlueprint.md](03-CppVsBlueprint.md) 6장 |
| 각 수치의 의미 | `systems/*.md` |

---

## 2. 데이터가 사는 네 곳

| 저장소 | 무엇을 두나 | 실제 위치 |
|---|---|---|
| **GameplayTag** | **식별자.** "이것이 무엇인가"를 가리키는 이름 | `Config/Tags/*.ini` |
| **DataTable** | **행이 여럿인 밸런싱 수치.** 같은 모양의 데이터가 N 개 | `Content/HeavyHanded/Data/DataTables/` |
| **DeveloperSettings** | **전역 단일 값.** 시스템 전체에 하나뿐인 튜닝 | Project Settings → Game |
| **BP 프로퍼티** | **개체별 값.** 이 노획물, 이 경비 | BP 서브클래스 |

### 고르는 기준

두 가지만 물어보면 갈린다.

**1. 이름인가, 수치인가?**
이름이면 GameplayTag 다. 수치는 나머지 셋.
**태그에 수치를 넣지 않는다** — `Noise.Loot.Throw` 는 "던지기 소음"이라는 이름일 뿐,
그게 몇 %인지는 `DT_NoiseProfiles` 에 있다.

**2. 값이 몇 개인가?**

| 개수 | 저장소 |
|---|---|
| 전체에 하나 | DeveloperSettings |
| 같은 모양이 여러 행 | DataTable |
| 개체마다 다름 | BP 프로퍼티 |

경계도 감소율은 저택 전체에 하나뿐이므로 `UAlertSettings`.
소음 프로파일은 행동마다 한 행씩 있으므로 `DT_NoiseProfiles`.
석상의 무게는 석상마다 다르므로 BP.

구체 사례는 6장 판정표에 있다.

---

## 3. GameplayTag 체계

### 파일 구조

`Config/DefaultGameplayTags.ini` 에는 **설정만** 있고, 태그 본문은 시스템별 파일로 분리되어 있다.
머지 충돌 격리가 목적이다 — 6명이 한 파일에 태그를 추가하면 충돌이 상시화된다.

| 파일 | 담는 것 |
|---|---|
| `Tags/Noise.ini` | 소음 종류 (기획서 3장) |
| `Tags/Alert.ini` | 경계도 4단계 · 상승 원인 |
| `Tags/Role.ini` | 역할 4종 |
| `Tags/Ability.ini` | 스킬 16종 + 공통 소모품 + 쿨다운 |
| `Tags/State.ini` | 플레이어 상태 + 차단 태그(`Block.*`) |
| `Tags/Loot.ini` | 노획물 특성 4종 + 상태 |
| `Tags/Equipment.ini` | 은신처 장비 |
| `Tags/Guard.ini` | 경비 종류 · 상태 · 감지 |
| `Tags/Hazard.ini` | 환경 방해 요소 · 협력 장치 |
| `Tags/Phase.ini` | 게임 진행 단계 + 장소(`Site.*`) |
| `Tags/Cue.ini` | GameplayCue (연출) |
| `Tags/Event.ini` | GameplayEvent (GAS 트리거) |

각 파일의 담당자는 [06-Collaboration.md](06-Collaboration.md) 에 있다.
**`DefaultGameplayTags.ini` 에 태그를 직접 추가하지 않는다.**

### 함정 — `+` 접두사를 쓰면 안 된다

`Config/Tags/*.ini` 는 config 계층(hierarchy)이 아니라 **개별 파일로 직접 읽힌다.**
그래서 다른 `Default*.ini` 에서 쓰던 배열 추가 문법이 여기서는 통하지 않는다.

```ini
; 올바름
GameplayTagList=(Tag="Noise.Loot.Throw",DevComment="물건 던지기")

; 잘못됨 — 키 이름이 그대로 "+GameplayTagList" 로 저장되어
;          UGameplayTagsList 가 찾지 못하고, 경고 없이 태그 0개가 된다
+GameplayTagList=(Tag="Noise.Loot.Throw",DevComment="물건 던지기")
```

**실패가 조용하다.** 에러도 경고도 없이 그 파일의 태그가 전부 사라진다.
태그를 추가했는데 에디터에 안 보이면 여기부터 확인할 것.

### `DevComment` 는 비워두지 않는다

에디터 태그 선택기에 그대로 뜬다. 기획서 근거가 있으면 수치도 같이 적는다.

```ini
GameplayTagList=(Tag="Noise.Player.Run",DevComment="뛰기(Shift 홀드) — 소, +1%, 8m")
```

> 여기 적는 수치는 **설명이지 실제 값이 아니다.** 실제 값은 `DT_NoiseProfiles` 에 있다.
> 밸런싱으로 값이 바뀌면 DevComment 도 같이 고칠 것.

### 코드에서 태그를 쓸 때는 네이티브 선언

**태그를 문자열로 조회하면 오타를 컴파일러가 잡지 못하고 런타임에 조용히 실패한다.**
C++ 에서 참조하는 태그는 `Source/HeavyHanded/Public/Core/HeavyHandedGameplayTags.h` 의
`HHTags` 네임스페이스에 선언한다. 문자열 정의는 `.ini` 에 그대로 둔다 — 둘은 짝이다.

> **TODO:** `HHTags` 네임스페이스가 현재 비어 있다. 원래 있던 `Noise.*` 선언은 소음 파트로 소유가
> 옮겨가며 제거됐고(물리 파트는 태그 대신 `FLootImpactEvent` 를 방송한다), 아직 아무도 다시 채우지 않았다.
> C++ 에서 태그를 조회하는 코드가 생기면 여기부터 선언할 것.

### 태그 추가 절차

1. 해당 시스템의 `Config/Tags/<System>.ini` 에 `GameplayTagList=(...)` 한 줄 추가 (`+` 금지)
2. `DevComment` 작성
3. C++ 에서 참조한다면 `HHTags` 에 네이티브 선언 추가
4. 그 태그로 조회할 DataTable 행이 필요하면 같이 추가 (4장)
5. 남의 시스템 파일이면 담당자에게 먼저 알린다

---

## 4. DataTable

### `DT_NoiseProfiles`

현재 유일한 DataTable 이다. **기획서 부록의 소음 밸런싱 시트가 그대로 이 테이블이다.**

| 항목 | 값 |
|---|---|
| 경로 | `/Game/HeavyHanded/Data/DataTables/DT_NoiseProfiles` |
| 행 구조 | `FNoiseProfileRow` (`Noise/NoiseTypes.h`) |
| 가리키는 곳 | `UNoiseSettings::NoiseProfiles` (Project Settings → Game → Noise) |

### 규약 — `RowName` 은 GameplayTag 이름과 같게 한다

`UNoiseSubsystem::FindProfile()` 이 태그 이름으로 행을 찾는다.
정확히 일치하는 행이 없으면 **부모 태그로 거슬러 올라간다.**

```
Noise.Loot.Throw  ← 이 행이 있으면 사용
Noise.Loot        ← 없으면 여기로
Noise             ← 그래도 없으면 여기로
```

덕분에 세부 태그마다 행을 만들지 않아도 되고, 나중에 세분화할 때 상위 행을 두면 기본 동작이 유지된다.

**이 규약이 깨지면 태그는 있는데 소음이 안 나는 상태가 된다.**
없는 프로파일은 로그에 태그당 한 번만 경고한다 (물리 충돌 경로라 매번 찍으면 로그가 잠긴다).

### 행 구조를 에디터에서 강제한다

```cpp
UPROPERTY(config, EditAnywhere, Category = "Data",
    meta = (AllowedClasses = "/Script/Engine.DataTable",
            RequiredAssetDataTags = "RowStructure=/Script/HeavyHanded.NoiseProfileRow"))
TSoftObjectPtr<UDataTable> NoiseProfiles;
```

`RequiredAssetDataTags` 를 걸면 **행 구조가 다른 테이블은 선택 자체가 안 된다.**
새 DataTable 참조를 만들 때도 이 형태를 쓴다. 런타임 캐스트 실패로 알게 되는 것보다 낫다.

### 새 DataTable 을 만들 때

1. 행 구조체를 `FTableRowBase` 상속으로 C++ 에 정의
2. `Content/HeavyHanded/Data/DataTables/` 에 `DT_` 접두사로 생성
3. 참조는 `UDeveloperSettings` 의 `TSoftObjectPtr<UDataTable>` 로 (5·5장)
4. `RequiredAssetDataTags` 로 행 구조 강제

> `Content/HeavyHanded/Data/` 바로 아래가 아니라 반드시 `DataTables/` 하위에 둔다.
> 예전에 두 곳에 같은 이름의 테이블이 생겨 어느 쪽이 라이브인지 알 수 없었다. (2026-08-13 정리 완료)

---

## 5. DeveloperSettings — Project Settings

기획자가 만질 전역 값은 전부 여기 둔다. 에디터에서 바꾸고 `.ini` 에 저장된다.

| 클래스 | `config` | 파일 | 위치 |
|---|---|---|---|
| `UNoiseSettings` | `NoiseSystem` | `Config/DefaultNoiseSystem.ini` | Project Settings → Game → Noise |
| `UAlertSettings` | `AlertSystem` | `Config/DefaultAlertSystem.ini` | Project Settings → Game → Alert |
| `UUISettings` | `UI` | `Config/DefaultUI.ini` | Project Settings → UI |

> **TODO:** `Config/DefaultAlertSystem.ini` 가 아직 없다. `UAlertSettings` 는 현재 C++ 기본값만 쓴다.
> Project Settings 에서 한 번 저장하면 생성된다. **생성되면 반드시 커밋할 것** —
> 없으면 사람마다 다른 값으로 플레이테스트하게 된다.

### `Get()` 은 CDO 다 — null 검사를 하지 않는다

```cpp
static const UNoiseSettings* Get() { return GetDefault<UNoiseSettings>(); }
```

`GetDefault<T>()` 는 `T::StaticClass()->GetDefaultObject<T>()` 이고 **모듈 로드 시점에 이미 존재한다.**
절대 null 이 아니다.

```cpp
// 하지 말 것
const float Falloff = Settings ? Settings->DistanceFalloffExponent : 1.7f;
```

죽은 분기가 생기는 것도 문제지만, 진짜 문제는 **그 리터럴 `1.7f` 가 클래스 기본값과 따로 논다는 것**이다.
나중에 기본값을 바꿔도 이 자리는 조용히 옛 값으로 남는다.

### 수치를 하드코딩하지 않는다

`UAlertSettings` 주석 그대로 —

> **C++ / BP 어디에도 수치를 하드코딩하지 말 것 — 여기가 유일한 진리원이다.**

`UAlertComponent` 는 GameState 에 런타임 부착되어 **디테일 패널에 뜨지 않는다.**
그래서 임계값을 컴포넌트에 `EditAnywhere` 로 두면 아무도 만질 수 없다.
런타임 생성 컴포넌트의 수치는 항상 Settings 로 뺀다.

### 잘못된 값을 저장 시점에 막는다

```cpp
#if WITH_EDITOR
virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
```

`UAlertSettings` 는 이걸로 **히스테리시스가 뒤집힌 값**(`SuspiciousExit > SuspiciousEnter`)이
저장되지 않게 막는다. 값 사이에 지켜야 할 관계가 있으면 `ClampMin` 만으로는 부족하다.

### 조회 래퍼로 누락 키를 방어한다

```cpp
float GetSurfaceCoeff(EPhysicalSurface Surface) const
{
    const float* Found = SurfaceNoiseCoeff.Find(Surface);
    return Found ? *Found : 1.0f;
}
```

`TMap` 을 직접 노출하면 기획자가 행을 지웠을 때 **계수 0** 으로 떨어져 소음이 사라진다.
`TMap` 설정에는 래퍼를 붙이고 안전한 기본값을 돌려준다.

---

## 6. 에셋 참조와 경로

### 소프트 참조를 기본으로 한다

```cpp
TSoftObjectPtr<UDataTable> NoiseProfiles;          // UNoiseSettings
TSoftObjectPtr<UCurveFloat> ImpactCurve;           // FNoiseProfileRow
```

`UDeveloperSettings` 와 DataTable 행은 **모듈 로드 시점에 만들어진다.**
하드 참조로 두면 그 시점에 대상 에셋이 전부 로드되어 에디터 기동과 쿠킹이 느려진다.

**로드한 뒤에는 붙들어 둔다.** 매번 `LoadSynchronous()` 를 부르면 물리 충돌 경로에서 비용이 된다.

```cpp
UPROPERTY(Transient)
TObjectPtr<UDataTable> CachedProfileTable = nullptr;
bool bProfileTableResolved = false;
```

`bProfileTableResolved` 를 따로 두는 이유 — **로드 실패도 "해결됨"으로 기억해야** 매 호출마다
실패한 로드를 다시 시도하지 않는다. `nullptr` 검사만으로는 구별되지 않는다.

### 스캔 경로를 좁힌다

`Config/DefaultGame.ini` —

```ini
+GameplayCueNotifyPaths="/Game/HeavyHanded/Abilities/Cues"
+DirectoriesToAlwaysCook=(Path="/Game/HeavyHanded")
```

지정하지 않으면 엔진이 `/Game` 전체를 스캔해 **에디터 기동이 느려진다.**
**GameplayCue 애셋은 반드시 `/Game/HeavyHanded/Abilities/Cues` 아래에 둘 것.**
다른 곳에 두면 태그는 있는데 연출이 안 나오고, 그 이유가 어디에도 안 나온다.

> **TODO:** `AssetManagerSettings.PrimaryAssetTypesToScan` 이 비어 있다.
> 노획물 · 장비 · 역할 DataAsset 클래스가 생긴 뒤에 채운다.
> 지금 넣으면 존재하지 않는 클래스 참조로 기동 시 경고가 난다.

> **TODO:** `GlobalCurveTableName` 이 주석 처리되어 있다.
> 은신처 스킬 강화(레벨별 수치)를 붙일 때 `CT_AbilityScaling` 을 만들고 활성화한다.

---

## 7. 배치 판정표

*"이 값을 어디에 넣나."* 실제 사례로만 적는다.

| 값 | 저장소 | 구체 위치 |
|---|---|---|
| 뛰기 소음의 경계도 증가량 | DataTable | `DT_NoiseProfiles` 의 `Noise.Player.Run` 행 |
| 소음 전파 반경 | DataTable | 같은 행의 `Radius` |
| 거리 감쇄 지수 | DeveloperSettings | `UNoiseSettings::DistanceFalloffExponent` |
| 재질별 소음 계수 | DeveloperSettings | `UNoiseSettings::SurfaceNoiseCoeff` |
| 경계도 단계 임계값 | DeveloperSettings | `UAlertSettings::SuspiciousEnter` 등 |
| 경계도 자연 감소율 | DeveloperSettings | `UAlertSettings::DecayPerSecond` |
| HUD 금색 | DeveloperSettings | `UUISettings::GoldColor` |
| 이 석상의 무게 | BP 프로퍼티 | `BP_Loot_*` 의 `FLootPhysicsData::MassKg` |
| 이 노획물의 파손 임계 충격량 | BP 프로퍼티 | 같은 구조체의 `DamageImpulseThreshold` |
| 이 경비의 시야각 | BP 프로퍼티 | `BP_GuardBase` 파생의 `SightConfig` |
| 순찰 지점 목록 | 레벨 배치 | 레벨의 `AGuardAIController` 설정 |
| "던지기 소음"이라는 개념 | GameplayTag | `Tags/Noise.ini` |
| "과적 상태"라는 개념 | GameplayTag | `Tags/State.ini` |
| 과적일 때 속도 30% | BP 프로퍼티 | `GE_*` 에셋의 모디파이어 |

### 규칙

**같은 값이 두 곳에 있으면 반드시 어긋난다.**
어느 쪽이 원본인지 헷갈리는 상황이 생기면, 값을 옮기는 게 아니라 **참조하게 만든다.**

---

## 8. 이 문서를 고쳐야 할 때

- 새 DataTable · Settings 클래스를 만들면 4장 또는 5장 표에 추가한다
- 새 태그 파일을 만들면 3장 표에 추가하고 [06-Collaboration.md](06-Collaboration.md) 에 담당자를 적는다
- **다섯 번째 저장소**를 도입하려면(예: DataAsset) 2장에 먼저 추가하고 선택 기준을 적는다.
  네 곳으로 안 되는 이유가 분명할 때만
- 7장 판정표는 새 사례가 나올 때마다 한 줄씩 늘린다
