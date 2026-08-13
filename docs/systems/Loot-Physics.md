# 노획물 물리 · 운반 · 파손

| | |
|---|---|
| **담당** | 김민준 |
| **코드 위치** | ⚠️ **`feature/physics`** — `develop` 에 아직 병합되지 않았다 |
| **소스** | `Public|Private/Loot/`, `.../Interfaces/Carryable.h`, `.../Core/HeavyHandedTypes.h` |
| **태그** | `Config/Tags/Loot.ini` |
| **설정** | `Config/DefaultEngine.ini` — 콜리전 프로파일, `PhysicalSurfaces`, `PhysicsSettings` |
| **테스트** | `LootTestLevel` (`Content/Developers/MinJun/`) |

> **이 문서는 골격이다.** 표에 확인된 사실만 채워져 있고, 설명은 담당자가 채운다.
> `TODO` 표시가 채워야 할 자리다. 작성 형식은 [Noise-Alert.md](Noise-Alert.md) 를 참고할 것.

---

## 1. 이 시스템이 하는 일

기획서 5장 — 특성 3종 + 경보 연동형, 그리고 전 장소 공통 대형 금고.

> **TODO:** 한 문단으로. 특히 "물리적 사실만 알리고 해석은 소음 파트가 한다"는 경계 원칙을 적을 것.

---

## 2. 클래스 구성

| 클래스 | 부모 | 역할 |
|---|---|---|
| `ALootBase` | `AActor`, `ICarryable` | 노획물 본체. 물리 바디 · 운반 · 던지기 · 복제 |
| `ULootDurabilityComponent` | `UActorComponent` | 충격 누적과 파괴 판정 |
| `ICarryable` | `UInterface` | 운반 요청/허용 인터페이스 |

### 타입 (`Core/HeavyHandedTypes.h`)

| 타입 | 용도 |
|---|---|
| `FLootPhysicsData` | 무게 · 운반 · 던지기 · 파손 파라미터 (BP 가 값을 지정) |
| `FLootImpactEvent` | 충돌 1건의 **물리적 사실**. 소음 파트로 전달된다 |
| `FOnLootImpactSignature` | C++ 전용 멀티캐스트. **BP 에 열지 않는다** |
| `EWeightClass` | `Light` / `Normal` / `Heavy` |
| `ELootImpactCause` | `Drop` / `Throw` / `Collision` / `Break` |

### BP 연출 훅

| 함수 | 클래스 | 계약 |
|---|---|---|
| `OnValueChanged` | `ALootBase` | 표시만. 판정은 C++ 에서 끝났다 |
| `OnDamageAccumulated` | `ULootDurabilityComponent` | 머티리얼 · 메시 · 사운드만 |
| `OnBroken` | `ULootDurabilityComponent` | 파편 · 사운드. 메시는 이미 숨겨져 있다 |

---

## 3. 핵심 흐름

> **TODO:** 다음 흐름을 각각 다이어그램 또는 단계로. 순서 의존이 있는 곳을 명시할 것.
> - 집기 → 운반 → 놓기 / 던지기
> - 충격 누적 → 파괴
> - 2인 협력 캐리 (`PrimaryCarrier` 가 있는 이유 — "물건 하나에 두 플레이어가 물리 제약을 거는 방식은 네트워크에서 깨진다")

### `ApplyCarryState` — 순서가 중요하다

**런타임에는 물리 시뮬레이션 중인 컴포넌트를 웰드 없이 부착할 수 없다. 반드시 물리를 먼저 끈다.**
서버와 `OnRep` 이 같은 함수 하나만 쓴다 ([02-Networking.md](../02-Networking.md) 5장).

---

## 4. 데이터와 수치

| 무엇 | 어디 |
|---|---|
| 무게 · 운반 · 던지기 · 파손 파라미터 | `FLootPhysicsData` (BP 서브클래스가 지정) |
| 노획물 특성 · 상태 태그 | `Config/Tags/Loot.ini` |
| 콜리전 프로파일 `Loot` / `CarriedLoot` | `Config/DefaultEngine.ini` |
| 물리 재질 7종 | `Config/DefaultEngine.ini` `PhysicalSurfaces` |

### `CarriedLoot` 프로파일의 의도

캐릭터 채널만 `Block` 이고 월드는 `Overlap` 이다. 기획서 요구사항
**"플레이어의 실수로 다른 플레이어의 운반을 방해하는 요소"** 를 성립시키기 위한 것이다.
운반자 본인은 `IgnoreActorWhenMoving` 으로 런타임에 제외한다.

> **TODO:** `DamageImpulseThreshold` 실측표(10kg 기준 낙하 높이별)를 여기 옮길 것.
> 현재 헤더 주석에만 있다.

---

## 5. 네트워크

| 값 | 클래스 | 복제 |
|---|---|---|
| `PrimaryCarrier` | `ALootBase` | `ReplicatedUsing` |
| `CurrentValue` | `ALootBase` | `ReplicatedUsing` |
| `ImpactCount` | `ULootDurabilityComponent` | `Replicated` |
| `bIsBroken` | `ULootDurabilityComponent` | `Replicated` |

- `bReplicates = true` + `SetReplicateMovement(true)`
- `NetUpdateFrequency = 60` / `MinNetUpdateFrequency = 20`
- `SetPhysicsReplicationMode(PredictiveInterpolation)` — `BeginPlay` 에서
- 충돌 델리게이트는 **서버에서만** 바인딩한다

> **TODO:** `PredictiveInterpolation` 의 `(WIP)` 상태에 대한 폴백 스위치가 없다
> ([02-Networking.md](../02-Networking.md) 7장).

---

## 6. 다른 시스템과의 접점

| 상대 | 방향 | 수단 |
|---|---|---|
| 소음 (지성인) | 이쪽 → | `FLootImpactEvent`. **물리적 사실만 알린다** |
| 플레이어 · GAS (전영배) | 양방향 | `ICarryable`, `ABaseCharacter::SetHeldActor` |
| 환경 방해 요소 (오유석) | 양방향 | 압력판 · 컨베이어 |

**소음 등급을 이쪽에서 판단하지 않는다.** 무엇이 / 얼마의 충격으로 / 무슨 재질에 / 왜
부딪혔는지만 담고, 그게 얼마나 시끄러운지는 소음 파트가 해석한다.

---

## 7. 디버깅

| 수단 | 비고 |
|---|---|
| `ALootBase::bShowImpactDebug` | 액터별 스위치. 하위 컴포넌트가 `IsImpactDebugEnabled()` 로 따른다 |
| `DebugMinLogImpulse` | 이 값 미만은 출력하지 않는다 |
| `DebugRawHitCount` / `DebugConfirmedCount` | 게이팅 효과 확인용 |

액터별 스위치를 쓰는 이유는 [07-Testing.md](../07-Testing.md) 6장.

---

## 8. 알려진 제약과 TODO

> **TODO:** `Debug_ToggleGrabByLocalPlayer()`, `Debug_ThrowForward()`, `Debug_BeginThrowAim()`,
> `bDebugEnableTestKeys` 는 헤더 주석에 **"플레이어 파트가 연결되면 통째로 지운다"** 고 되어 있다.
> `feature/PlayerAndSkills` 에 `GAB_Interact` · `GAB_Throw` 가 이미 있으므로 조건이 충족됐다.
> 병합 시점에 제거할 것.

> **TODO:** `feature/physics` 가 `develop` 보다 30 커밋 뒤처져 있다.
> `Config/DefaultEngine.ini` 를 양쪽에서 건드리므로 병합 전에 `develop` 을 먼저 당겨올 것
> ([06-Collaboration.md](../06-Collaboration.md) 3장).

> **TODO:** 기획서 5장 중 미착수 — 불안정형 유출(기울기 60도), 경보 연동형 좌대 카운트다운,
> 대형 금고 절단.
