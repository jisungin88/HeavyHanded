# 05. 코딩 컨벤션 및 폴더 구조

> **이 문서가 팀 컨벤션의 원본이다.**
> 기존 Notion "팀 컨벤션" 페이지의 2장(C++ 컨벤션)과 4-1·4-2(에셋 접두사·폴더)를 여기로 옮겼다.
> Notion 페이지는 이 문서를 가리키는 입구로만 남긴다.
> 코드 주석의 `(컨벤션 2-2)` 같은 참조는 아래 절 번호로 읽으면 된다.

| 옛 번호 | 현재 위치 |
|---|---|
| 2-1 명명 규칙 | 이 문서 2장 |
| 2-2 클래스 접두사 | 이 문서 2장 |
| 2-3 UPROPERTY / UFUNCTION | 이 문서 4장 |
| 2-4 전방 선언 | 이 문서 5장 |
| 2-5 UObject 포인터 / GC | 이 문서 6장 |
| 3-x 네트워크 규칙 | [02-Networking.md](02-Networking.md) |
| 4-1 에셋 접두사 | 이 문서 9장 |
| 4-2 폴더 규칙 | 이 문서 10장 |
| 4-3 충돌 방지, 4-4 Git 순서, 1장 커밋 메시지 | [06-Collaboration.md](06-Collaboration.md) |

---

## 1. 이 문서의 범위

**여기서 정하는 것** — 이름, 매크로 사용법, 헤더 작성법, 포인터 안전, 주석, 인코딩, 폴더 구조.

**여기서 정하지 않는 것**

| 주제 | 문서 |
|---|---|
| 복제·RPC 를 **언제 어떻게 쓰는가** (이름 규칙은 여기 3장) | [02-Networking.md](02-Networking.md) |
| C++ / BP 경계 | [03-CppVsBlueprint.md](03-CppVsBlueprint.md) |
| 커밋 메시지, 브랜치, 에셋 충돌 방지 | [06-Collaboration.md](06-Collaboration.md) |

---

## 2. C++ 명명 규칙

### 기본

- **파스칼 케이스(PascalCase)** 를 쓴다. 카멜 케이스는 쓰지 않는다.
- **bool 변수에는 소문자 `b` 접두사**를 붙이고 뒤는 파스칼 케이스로 쓴다. (`bIsDead`, `bCanAttack`, `bReplicates`)

### 클래스 접두사

언리얼의 클래스 접두사는 취향이 아니라 **UHT(Unreal Header Tool)가 검사하는 규칙**이다.

| 접두사 | 대상 | 예시 |
|---|---|---|
| `A` | `AActor` 상속 | `ALootBase`, `AGuardCharacter` |
| `U` | `UObject` 상속 (컴포넌트 포함) | `UAlertComponent`, `UNoiseSubsystem` |
| `F` | 일반 구조체 / 클래스 (UObject 아님) | `FNoiseEvent`, `FLootImpactEvent` |
| `E` | 열거형 | `EAlertLevel`, `ENoiseGrade` |
| `I` | 인터페이스 | `INoiseListener`, `ICarryable` |
| `T` | 템플릿 | `TArray`, `TObjectPtr`, `TSubclassOf` |

### 추가 규칙

- **파일명은 접두사를 뺀 클래스명**과 같게 한다. `ALootBase` → `LootBase.h` / `LootBase.cpp`
- 열거형은 `enum class ... : uint8` 로 선언한다. (`UPROPERTY` / 블루프린트 노출 조건)

```cpp
UENUM(BlueprintType)
enum class EAlertLevel : uint8
{
    Calm        UMETA(DisplayName = "평온"),
    Suspicious  UMETA(DisplayName = "의심"),
    Alerted     UMETA(DisplayName = "경계"),
    Alarm       UMETA(DisplayName = "경보")
};
```

---

## 3. RPC 함수 네이밍

**RPC 함수명에는 종류 접두사를 붙인다.** 호출부에서 이게 네트워크를 타는 호출인지
한눈에 보여야 하기 때문이다.

```cpp
UFUNCTION(Server, Reliable, WithValidation)
void Server_SendChatMessage(const FString& Message);

UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayBreakVFX(FVector Location);

UFUNCTION(Client, Reliable)
void Client_ShowAlarmCountdown(float Seconds);
```

어떤 RPC 를 **언제 쓰는가**는 [02-Networking.md](02-Networking.md) 4장에 있다.

### `BlueprintCallable` RPC 의 이름을 바꿀 때

**BP 에서 호출 중인 `UFUNCTION` 의 이름을 바꾸면 BP 노드가 끊긴다.**
`.uasset` 은 바이너리라 C++ 쪽에서 고칠 수 없고, 담당자가 에디터에서 노드를 다시 연결해야 한다.

이름을 바꿔야 한다면 **커밋 메시지에 어떤 BP 를 고쳐야 하는지 적고 담당자에게 알린다.**
바꾸지 않기로 했다면 그 이유를 헤더 주석에 남긴다.

---

## 4. UPROPERTY / UFUNCTION 매크로

### `Category` 는 필수다

에디터 디테일 창과 블루프린트 노드 검색이 정리되지 않으면 값이 늘어날수록 못 쓰게 된다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats")
float MaxHealth;

UFUNCTION(BlueprintCallable, Category = "Player Action")
void Attack();
```

계층이 필요하면 `|` 로 나눈다 — `Category = "Loot|Throw"`, `Category = "Guard|Patrol"`.

### 노출 수준을 명시한다

| 특성 | 언제 |
|---|---|
| `EditAnywhere` | 아키타입과 인스턴스 양쪽에서 조정 |
| `EditDefaultsOnly` | BP 기본값으로만 조정. 레벨에 놓인 개체마다 달라지면 안 되는 값 |
| `VisibleAnywhere` | 확인만. 컴포넌트 포인터 등 |
| `BlueprintReadOnly` | **기본값으로 쓴다.** BP 는 에디터에서 지정하고 런타임에는 바꾸지 않는다 |
| `BlueprintReadWrite` | 런타임 변경이 정말 필요할 때만 |

`BlueprintReadOnly` 를 기본으로 하는 이유는 [03-CppVsBlueprint.md](03-CppVsBlueprint.md) 6장에 있다.

### 값 제약을 메타로 건다

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Physics",
    meta = (ClampMin = "0.1", Units = "cm"))
float MassKg = 10.f;
```

`ClampMin` / `ClampMax` / `Units` 를 붙여두면 잘못된 값이 애초에 들어오지 못한다.
문서로 막는 것보다 확실하다.

---

## 5. 헤더 파일 최적화 — 전방 선언

**`.h` 안에서 `#include` 를 최소화한다.** 다른 클래스를 참조할 때는 전방 선언을 쓰고,
실제 `#include` 는 `.cpp` 에서 한다. 빌드 시간이 걸린 문제다.

```cpp
// MyCharacter.h
#pragma once

class UCameraComponent;   // 전방 선언

UCLASS()
class AMyCharacter : public ACharacter
{
    GENERATED_BODY()

private:
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> CameraComp;
};
```

### 전방 선언이 불가능한 경우

`#include` 해야 한다. 그리고 **왜 include 했는지 한 줄 남긴다** — 다음 사람이
"이건 전방 선언으로 바꿀 수 있는데?" 하고 건드렸다가 컴파일을 깨뜨리지 않게.

| 경우 | 예시 |
|---|---|
| 값(value) 타입으로 들고 있는 구조체 | `NoiseSubsystem.h` 의 `FHitResult` — 스크래치 버퍼를 값으로 보유 |
| 상속하는 부모 클래스 | — |
| `UPROPERTY` 에 노출하는 enum | `AlertComponent.h` 의 `EAlertLevel` |

---

## 6. UObject 포인터와 가비지 컬렉션

> **UObject 계열 포인터 멤버에는 예외 없이 `UPROPERTY()` 를 붙인다.
> 이것은 스타일 규칙이 아니라 크래시 방지 규칙이다.**

`UPROPERTY()` 가 없는 포인터는 GC 에게 "아무도 참조하지 않는 객체"로 보인다.
언리얼이 객체를 회수한 뒤 **몇 분 후 전혀 다른 코드에서 크래시**가 난다.
재현이 어렵고 원인 추적에 며칠이 걸리는 유형의 버그다.

```cpp
// 잘못된 예 — 언젠가 반드시 크래시가 난다
UAlertComponent* AlertComp;

// 올바른 예 — 에디터에 노출할 필요가 없어도 빈 UPROPERTY() 라도 붙인다
UPROPERTY()
TObjectPtr<UAlertComponent> AlertComp;
```

### 추가 규칙

- UE5 에서는 원시 포인터(`UObject*`) 대신 **`TObjectPtr<T>`** 를 쓴다.
- 사용 전 **`IsValid()`** 로 검사한다. 특히 다른 플레이어나 노획물을 참조할 때는 필수다 —
  그 액터가 이미 파괴됐거나 플레이어가 접속을 끊었을 수 있다.

```cpp
if (IsValid(TargetLoot))
{
    TargetLoot->OnPickedUp();
}
```

### 약참조를 써야 하는 경우

큐나 맵에 잠시 보관되는 참조는 **`TWeakObjectPtr<T>`** 로 둔다.
하드 레퍼런스로 잡으면 파괴된 객체가 GC 되지 않는다.
`FLootImpactEvent::LootActor`, `FNoiseEvent::InstigatorActor` 가 이 경우다.

> `TWeakObjectPtr` 를 담은 구조체는 BP 에 노출하지 않는다. 수명 관리가 깨진다.

---

## 7. 주석 규칙

### 기본 — 무엇을 하는가

**public 클래스·함수·프로퍼티에는 무엇을 하는 것인지 한 줄을 단다.**
헤더만 읽고도 쓸 수 있어야 한다. 구현을 열어봐야 용도를 알 수 있으면 주석이 부족한 것이다.

```cpp
/** 다음 순찰 지점을 골라 Blackboard 의 PatrolLocation 에 써넣는다 */
UFUNCTION(BlueprintCallable, Category = "Guard|Patrol")
void SelectNextPatrolPoint();

/** 소지 중 이동 속도 배율 (기획서: 중량형 1인 시 0.3) */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Carry")
float CarrySpeedMultiplier = 1.f;
```

- `/** ... */` 형식을 쓴다. 에디터 툴팁과 IDE 힌트에 그대로 뜬다.
- 기획서에 근거가 있는 수치는 **어느 장인지** 같이 적는다.
- 코드를 읽으면 바로 아는 것은 적지 않는다. (`// i 를 1 증가시킨다`)

### 다음 세 경우에는 "왜"도 같이 적는다

주석이 필요 없는 코드가 가장 좋지만, 아래는 코드만으로 전달되지 않는다.

1. **순서가 중요한 곳** — 예: 물리를 끄기 전에 부착하면 엔진이 거부한다
2. **직관과 어긋나는 값이나 선택** — 예: `CarrierVelocityInfluence` 기본값이 물리적으로 맞는 1 이 아니라 0 인 이유
3. **실측으로 정한 임계값** — 어떻게 쟀는지. 예: `DamageImpulseThreshold` 의 낙하 높이별 실측표

이 셋은 나중에 누군가 "이거 왜 이래?" 하고 **되돌리는** 지점이다.
근거가 없으면 같은 실수가 반복된다.

---

## 8. 파일 인코딩과 포맷

설정은 `.editorconfig` 에 있고 Visual Studio · Rider · VS Code 가 모두 이 파일을 읽는다.
**아래는 설명이고, 실제 강제는 `.editorconfig` 가 한다.**

| 대상 | 들여쓰기 | 인코딩 | 줄바꿈 |
|---|---|---|---|
| `.h` `.cpp` `.inl` `.cs` | 탭, 폭 4 | **UTF-8 with BOM** | LF |
| `.ini` | 공백 4 | UTF-8 | LF |
| `.yml` `.json` | 공백 2 | UTF-8 | LF |
| `.bat` `.ps1` | — | UTF-8 | CRLF |

### BOM 이 필수인 이유

`.editorconfig` 주석 그대로 —

> BOM 이 없으면 MSVC 가 소스를 시스템 코드페이지(여기서는 CP949)로 읽는다.
> 주석의 한글이 C4819 를 내고, 한글 바이트 끝이 `0x5C` 로 잡히면 줄 이음으로 먹혀
> **다음 줄이 통째로 주석이 되는 일까지 생긴다.** UBT 는 `/utf-8` 을 붙여주지 않는다.

**한글 주석이 든 소스는 반드시 UTF-8 BOM 으로 저장한다.**
새 파일을 만들 때 IDE 설정을 확인할 것. 리포에 인코딩 복구 커밋이 이미 다섯 번 있었다.

### `.ini` 는 `trim_trailing_whitespace` 를 끈다

엔진이 `.ini` 를 그대로 다시 쓰기 때문에, 정리해봐야 다음 저장에서 되돌아온다.

---

## 9. 에셋 접두사

| 접두사 | 종류 | 접두사 | 종류 |
|---|---|---|---|
| `BP_` | 블루프린트 | `SM_` | 스태틱 메시 |
| `WBP_` | 위젯 블루프린트 | `SK_` | 스켈레탈 메시 |
| `M_` | 머티리얼 | `T_` | 텍스처 |
| `MI_` | 머티리얼 인스턴스 | `S_` | 사운드 웨이브 |
| `DT_` | 데이터 테이블 | `SC_` | 사운드 큐 |
| `DA_` | 데이터 애셋 | `NS_` | 나이아가라 시스템 |
| `BB_` / `BT_` | 블랙보드 / 비헤이비어 트리 | `IA_` / `IMC_` | Input Action / Mapping Context |
| `GA_` | GameplayAbility | `GE_` | GameplayEffect |

예 — `BP_GuardDog`, `WBP_AlertGauge`, `SM_SafeLarge`, `S_Noise_Drop_Heavy`, `DT_NoiseProfiles`

---

## 10. 폴더 구조

### 소스

`Public/` 과 `Private/` 이 **같은 하위 구조**를 갖는다. 헤더를 찾을 때 경로를 그대로 바꿔 쓰면 된다.

```
Source/HeavyHanded/
├── Public/            ← 헤더
│   ├── AI/            경비 AI · BT 노드
│   ├── Alert/         경계도
│   ├── Character/     플레이어 · 경비 캐릭터 · 어빌리티
│   ├── Core/          GameMode · GameState · PlayerController · PlayerState
│   ├── Data/          데이터 자산
│   ├── Equipment/     장비
│   ├── Hazards/       환경 방해 요소
│   ├── Interaction/   상호작용
│   ├── Interfaces/    인터페이스
│   ├── Loot/          노획물
│   ├── Noise/         소음
│   ├── Shared/        여러 시스템이 함께 쓰는 것 (NetAuthority.h, Gauge01.h)
│   └── UI/            위젯 베이스
└── Private/           ← 구현. Public 과 같은 구조
    └── Tests/         Automation Test
```

`Shared/` 는 **여러 시스템이 같은 식을 써야 하는 것만** 넣는다. 잡동사니 폴더가 아니다.
지금 있는 두 파일 다 "복사본이 갈라져서 사고가 났던 것"을 한곳에 모은 결과다.

### 콘텐츠

- **우리가 만드는 에셋은 `Content/HeavyHanded/` 하위에만** 배치한다.
  마켓플레이스·플러그인 에셋과 섞이지 않게 하기 위함이다.
- **검증되지 않은 개인 실험물은 `Content/Developers/<본인이름>/`** 에서 작업하고,
  완성된 뒤 정식 폴더로 옮긴다. 정식 경로에서 `Developers/` 아래를 참조하지 않는다.
- **폴더 이동과 이름 변경은 반드시 언리얼 에디터 안에서 한다.**
  윈도우 탐색기에서 옮기면 다른 에셋의 참조가 전부 깨진다.

```
Content/HeavyHanded/
├── Art/          Materials · Meshes · Textures · Decals
├── Audio/        SFX (Guard · Loot · Noise · Player · UI) · Music · Mix
├── Characters/   Player · Guards (Guard · GuardDog · ArmedGuard · AI)
├── Core/         GameModes · GameStates · PlayerControllers · PlayerStates · Session
├── Data/         DataTables · DataAssets · Curves
├── Equipment/    장비
├── FX/           Niagara · Materials
├── Hazards/      Detection · Periodic · ForcedMovement
├── Input/        IA · IMC
├── Loot/         Heavy · Fragile · Unstable · AlarmLinked
├── Maps/         Mansion · Museum · Bank · Hideout · Lobby · Test
├── UI/           HUD · Lobby · Hideout · Result · Common · Icons · Fonts
└── Vehicles/     밴
```

### 설정

`Config/Tags/*.ini` 는 **담당자별로 분리되어 있다.** 머지 충돌 격리가 목적이다.
배정표는 [06-Collaboration.md](06-Collaboration.md) 에 있다.

---

## 11. 이 문서를 고쳐야 할 때

- 이 문서가 **팀 컨벤션의 유일한 원본이다.** 규칙이 바뀌면 여기를 고치고, 다른 곳에 복사하지 않는다
- 새 에셋 종류를 도입하면 9장 표에 접두사를 추가한다
- 규칙과 실제 코드가 어긋난 것을 발견하면, **어느 쪽이 맞는지 정하고 한쪽을 고친다.**
  어긋난 채로 두면 두 규칙이 생기고 아무도 안 지키게 된다
