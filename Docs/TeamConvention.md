# 팀 컨벤션 — SNEAKERS

> 출처: [Notion · 팀 컨벤션](https://app.notion.com/p/3b2aee16de2380d69049f06b34c9c26c)
> UE 5.4 / Listen Server / 6인 팀
> **Notion이 원본입니다.** 원본이 바뀌면 이 문서도 갱신하세요.

---

## 0. 어기면 안 되는 핵심 규칙

- UObject 포인터 멤버에는 **예외 없이** `UPROPERTY()`를 붙인다 (크래시 방지 규칙)
- 게임 상태를 바꾸는 코드는 **반드시 서버에서만** 실행한다 (`HasAuthority()` 체크)
- 복제 변수는 **반드시** `GetLifetimeReplicatedProps()`에 등록한다
- `.uasset` / `.umap`은 Git이 병합할 수 없다 — 같은 레벨/블루프린트를 동시에 열지 않는다

---

## 1. Git 커밋 메시지 규약

| 타입 | 설명 | 예시 |
|---|---|---|
| `feat` | 새로운 기능 추가 | feat: 로그인 기능 추가 |
| `fix` | 버그 수정 | fix: 결제 요청 시 네트워크 에러 수정 |
| `docs` | 문서 수정 (README 등) | docs: API 설명서 업데이트 |
| `style` | 코드 포맷팅, 세미콜론 누락 등 (로직 변경 없음) | style: 변수명 오타 수정 및 줄바꿈 정리 |
| `refactor` | 리팩토링 (기능 변경 없이 코드 구조 개선) | refactor: 로그인 로직 성능 개선 |
| `test` | 테스트 코드 추가 또는 수정 | test: 회원가입 단위 테스트 작성 |
| `chore` | 빌드 업무, 패키지 매니저 설정, .gitignore 수정 등 | chore: .gitignore에 LFS 규칙 추가 |

작성 예시:

```
fix: 캐릭터 점프 중 충돌 체크 오류 수정

- 공중 상태에서 두 번 점프되는 판정 버그 수정
- GroundCheck 레이캐스트 거리값 조정 (0.5f -> 0.2f)
```

---

## 2. UE5 C++ 코딩 컨벤션

### 2-1. 명명 규칙

- **파스칼 케이스(PascalCase)** 사용. 카멜 케이스는 쓰지 않는다.
- `bool` 변수명 앞에는 반드시 소문자 `b`를 붙이고 파스칼 케이스로 작성한다. (`bIsDead`, `bCanAttack`)

### 2-2. 클래스 접두사

언리얼의 클래스 접두사는 취향이 아니라 **UHT가 검사하는 규칙**입니다.

| 접두사 | 대상 | 예시 |
|---|---|---|
| `A` | AActor 상속 클래스 | `AGuardBase`, `ALootBase` |
| `U` | UObject 상속 클래스 (컴포넌트 포함) | `UAlertComponent`, `UCarryComponent` |
| `F` | 일반 구조체 / 클래스 (UObject 아님) | `FNoiseEvent`, `FLootData` |
| `E` | 열거형 (enum) | `EAlertLevel`, `ENoiseGrade` |
| `I` | 인터페이스 | `IInteractable` |
| `T` | 템플릿 | `TArray`, `TObjectPtr`, `TSubclassOf` |

- 파일명은 **접두사를 뺀 클래스명**과 동일하게 작성한다. (`AGuardBase` → `GuardBase.h` / `GuardBase.cpp`)
- 열거형은 `enum class ... : uint8`로 선언한다. (UPROPERTY / 블루프린트 노출 조건)

```cpp
UENUM(BlueprintType)
enum class EAlertLevel : uint8
{
    Calm           UMETA(DisplayName = "평온"),
    Suspicious     UMETA(DisplayName = "의심"),
    Alerted        UMETA(DisplayName = "경계"),
    Alarm          UMETA(DisplayName = "경보")
};
```

### 2-3. UPROPERTY / UFUNCTION

에디터 노출 수준과 블루프린트 연동 속성을 명확히 지정하고, **`Category`는 반드시 작성**한다.

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats")
float MaxHealth;

UFUNCTION(BlueprintCallable, Category = "Player Action")
void Attack();
```

### 2-4. 헤더 최적화 (전방 선언)

빌드 속도를 위해 `.h`에서는 `#include` 대신 전방 선언을 쓰고, 실제 `#include`는 `.cpp`에서 한다.

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

전방 선언이 **불가능한** 경우 → `#include` 한다:
값(value) 타입으로 들고 있는 구조체, 상속하는 부모 클래스, UPROPERTY에 노출하는 enum.

### 2-5. UObject 포인터와 GC

UObject 계열 포인터 멤버에는 **예외 없이** `UPROPERTY()`를 붙인다. 스타일 규칙이 아니라 **크래시 방지 규칙**이다.
`UPROPERTY()`가 없는 포인터는 GC에게 "아무도 참조하지 않는 객체"로 보이므로, 회수된 뒤 **몇 분 후 전혀 다른 코드에서 크래시**가 난다.

```cpp
// 잘못된 예 — 언젠가 반드시 크래시가 난다
UAlertComponent* AlertComp;

// 올바른 예 — 에디터에 노출할 필요가 없어도 빈 UPROPERTY()라도 붙인다
UPROPERTY()
TObjectPtr<UAlertComponent> AlertComp;
```

- UE5에서는 원시 포인터(`UObject*`) 대신 **`TObjectPtr<T>`**를 사용한다.
- 사용 전 `IsValid()`로 검사한다. 다른 플레이어나 노획물을 참조할 때는 **필수**다.

```cpp
if (IsValid(TargetLoot))
{
    TargetLoot->OnPickedUp();
}
```

---

## 3. 네트워크 규칙 (Listen Server)

이 프로젝트는 **Listen Server 방식의 멀티플레이 게임**입니다.
아래는 "지키면 좋은 것"이 아니라 **어기면 게임이 성립하지 않는 조건**입니다.

### 3-1. 권한(Authority) 원칙

게임 상태를 바꾸는 코드는 반드시 서버에서만 실행한다. 함수 진입부에서 권한을 먼저 확인한다.

```cpp
void ALootBase::ApplyDamage(int32 Amount)
{
    if (!HasAuthority())
    {
        return;
    }

    // 여기부터는 서버에서만 실행된다
    ImpactCount += Amount;
}
```

**소음 판정은 서버 전용이다.** 기획서 명시 사항. 클라이언트가 물리 충돌 이벤트를 서버에 보고하는 경로를 만들지 않는다. 클라이언트마다 물리 시뮬레이션 결과가 미세하게 다르므로, 클라이언트를 신뢰하면 사람마다 다른 소음이 발생한다.

### 3-2. RPC 규칙

| 종류 | 용도 | 주의 |
|---|---|---|
| Server | 클라이언트 → 서버 **요청** | 결과 판정은 서버가 한다. 클라이언트를 신뢰하지 않는다. |
| Multicast | 서버 → 전원 **연출** 재생 | 연출 전용. 안에서 게임 상태를 바꾸지 않는다. |
| Client | 서버 → 특정 클라이언트 알림 | 해당 플레이어에게만 필요한 UI 갱신 등 |

RPC 함수명에는 종류 접두사를 붙인다.

```cpp
UFUNCTION(Server, Reliable, WithValidation)
void Server_RequestPickupLoot(ALootBase* Loot);

UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayBreakVFX(FVector Location);

UFUNCTION(Client, Reliable)
void Client_ShowAlarmCountdown(float Seconds);
```

- 연출용 Multicast는 **Unreliable**. 파티클 한 번 누락되는 것보다 대역을 아끼는 편이 낫다.
- 게임 진행에 영향을 주는 RPC는 **Reliable**.

### 3-3. 복제 변수

복제 변수는 반드시 `GetLifetimeReplicatedProps()`에 등록한다.
**등록을 빠뜨려도 컴파일 에러도 경고도 나지 않는다.** 호스트에서는 정상 동작하고 클라이언트에서만 값이 갱신되지 않기 때문에 발견이 늦어지는 대표적인 실수다.

```cpp
// AlertComponent.h
UPROPERTY(ReplicatedUsing = OnRep_AlertLevel)
float AlertGauge;

UFUNCTION()
void OnRep_AlertLevel();

// AlertComponent.cpp
#include "Net/UnrealNetwork.h"

void UAlertComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UAlertComponent, AlertGauge);
}
```

- 액터가 복제되어야 하면 생성자에서 `bReplicates = true;`
- 컴포넌트는 `SetIsReplicatedByDefault(true);`
- HUD 갱신처럼 "값이 바뀌었을 때 반응"이 필요하면 `ReplicatedUsing`(RepNotify) 사용

### 3-4. 테스트 규칙

모든 기능은 **처음부터 멀티 환경에서** 테스트한다.
Play 설정 → Number of Players: 2 이상 → Run Under One Process: **해제** → Net Mode: **Play As Listen Server**

- **호스트 창에서만 확인하지 않는다.** 호스트는 지연이 0이라 물리 운반 동기화 문제가 100% 정상으로 보인다. 반드시 클라이언트 창에서 확인한다.
- 싱글플레이로 먼저 만들고 나중에 복제를 붙이는 방식은 대부분 전면 재작성으로 끝난다.

---

## 4. 에셋 규칙

`.uasset` / `.umap`은 바이너리라 **Git이 병합할 수 없다.** 코드 충돌은 해결할 수 있지만 에셋 충돌은 **한쪽 작업이 그대로 소실**된다.

### 4-1. 에셋 접두사

| 접두사 | 종류 | 접두사 | 종류 |
|---|---|---|---|
| `BP_` | 블루프린트 | `SM_` | 스태틱 메시 |
| `WBP_` | 위젯 블루프린트 | `SK_` | 스켈레탈 메시 |
| `M_` | 머티리얼 | `T_` | 텍스처 |
| `MI_` | 머티리얼 인스턴스 | `S_` | 사운드 웨이브 |
| `DT_` | 데이터 테이블 | `SC_` | 사운드 큐 |
| `DA_` | 데이터 애셋 | `NS_` | 나이아가라 시스템 |
| `BB_` / `BT_` | 블랙보드 / 비헤이비어 트리 | `IA_` / `IMC_` | Input Action / Mapping Context |

예: `BP_GuardDog`, `WBP_AlertGauge`, `SM_SafeLarge`, `S_Noise_Drop_Heavy`, `DT_LootBalance`

### 4-2. 폴더 규칙

- 우리가 만드는 에셋은 **`Content/HeavyHanded/` 하위에만** 배치한다. (마켓플레이스/플러그인 에셋과 섞이지 않게)
- 검증되지 않은 개인 실험물은 **`Content/Developers/<본인이름>/`**에서 작업하고, 완성 후 정식 폴더로 옮긴다.
- **폴더 이동 및 이름 변경은 반드시 언리얼 에디터 안에서 수행한다.** 윈도우 탐색기에서 옮기면 다른 에셋의 참조가 전부 깨진다.

### 4-3. 충돌 방지 (가장 중요)

- **레벨(.umap)은 두 명이 동시에 열지 않는다.** 작업 시작 전 팀 채팅에 "Mansion 작업 진행합니다.", 끝나면 "Mansion 작업 완료했습니다."를 남긴다.
- 같은 블루프린트를 두 사람이 수정하지 않는다. 시스템별 담당자를 정해두고, 남의 영역을 고쳐야 하면 담당자에게 먼저 알린다.
- 로직은 가능한 한 **C++로** 작성한다. C++은 diff와 병합이 가능하지만 블루프린트는 불가능하므로 충돌 비용이 비교할 수 없이 낮다. 블루프린트는 C++ 클래스를 상속해 **값(수치·메시·사운드)만 지정하는 껍데기**로 유지한다.

### 4-4. Git 작업 순서

```
1. 에디터에서 전체 저장 (Ctrl + Shift + S)
2. 에디터를 닫는다          ← pull 전에 반드시
3. git pull
4. 에디터를 다시 열고 작업
5. 커밋 전 다시 전체 저장 후 에디터를 닫는다
6. 커밋 후 푸시
```

- 에디터가 켜진 채로 `git pull` 하면 파일이 잠겨 있거나, **메모리에 남아 있던 구버전이 나중에 저장**되면서 받아온 내용을 덮어쓴다.
- **Git LFS 설치는 clone 전에 각자 1회 필수.** (`git lfs install`) 빠뜨린 사람이 올린 에셋만 LFS를 타지 않고 실제 바이너리로 들어가며, 나중에 골라내려면 히스토리 재작성이 필요하다.
