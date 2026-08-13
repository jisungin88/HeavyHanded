# 플레이어 조작 · GAS · 어빌리티

| | |
|---|---|
| **담당** | 전영배 |
| **코드 위치** | ⚠️ **`feature/PlayerAndSkills`** — `develop` 에 아직 병합되지 않았다 |
| **소스** | `Public|Private/Character/`, `.../Character/Ability/` |
| **태그** | `Config/Tags/Role.ini`, `Ability.ini`, `State.ini` |
| **테스트** | `Untitled` (`Content/Developers/Doom/`) |

> **이 문서는 골격이다.** 표에 확인된 사실만 채워져 있고, 설명은 담당자가 채운다.
> 작성 형식은 [Noise-Alert.md](Noise-Alert.md) 를 참고할 것.

---

## 1. 이 시스템이 하는 일

기획서 4장 — 조작과 상태, 4-1장 — 역할 4종 × 4슬롯 = 16 스킬.

> **TODO:** 한 문단으로.

---

## 2. 클래스 구성

| 클래스 | 부모 | 역할 |
|---|---|---|
| `ABaseCharacter` | `ACharacter`, `IAbilitySystemInterface` | 이동 · 입력 · 운반(`HeldActor`) |
| `APlayerSessionState` | `APlayerState`, `IAbilitySystemInterface` | **ASC · AttributeSet 소유**, `Gold`, `SelectedCharacterID` |
| `UBaseAttributeSet` | `UAttributeSet` | `Health`, `MovementSpeed` |
| `UBaseGameplayAbility` | `UGameplayAbility` | 어빌리티 공통 베이스 (몽타주 · 이벤트 전송) |
| `UGAB_Interact` | `UBaseGameplayAbility` | 집기 · 장치 조작 (시선 기준 트레이스) |
| `UGAB_Throw` | `UBaseGameplayAbility` | 조준 · 던지기 |
| `UGAB_DropItem` | `UBaseGameplayAbility` | 놓기 |

### 타입

`FAbilityInputBinding` — 에디터에서 `UInputAction` ↔ `TSubclassOf<UGameplayAbility>` 를
1:1로 매핑한다. 실제 매핑은 BP 가 채운다.

### BP 에셋

`GA_Interact` `GA_Throw` `GA_DropItem` (어빌리티 서브클래스),
`GE_Sprint` `GE_Crouch` (순수 BP), `BP_Brute` `BP_TestCharacter`,
Input Action 9종 + `IMC_Default`

역할 4종 메시 — `Char_Brute` `Char_Ghost` `Char_Oracle` `Char_Mimic`

---

## 3. 핵심 흐름

> **TODO:** 다음을 각각. 특히 서버·클라 경로가 갈리는 지점을 명시할 것.
> - 입력 → 어빌리티 활성화
> - 집기 (`GAB_Interact` → `SetHeldActor`)
> - 던지기 (조준 → 임펄스 → `BlockRecatch`)
> - 상태 부여/해제 (`GE_Sprint`, `GE_Crouch`)

### 운반 상태 적용

`ABaseCharacter::ApplyCarryState()` 가 물리/콜리전 게이팅과 부착/분리를 한 묶음으로 처리한다.
**서버와 `OnRep_HeldActor` 가 이 함수 하나만 쓴다** ([02-Networking.md](../02-Networking.md) 5장).

### 재획득 금지

던진 뒤 `RecatchBlockSeconds`(기본 0.75초) 동안 **던진 본인만** 다시 잡지 못한다.
던지고 낚아채기를 반복하면 중량형의 "2인 필수" 규칙과 착지 소음·파손 판정을 통째로 우회할 수 있다.
동료에게 패스하는 것은 막지 않는다.

---

## 4. 데이터와 수치

| 무엇 | 어디 |
|---|---|
| 역할 4종 | `Config/Tags/Role.ini` |
| 스킬 16종 + 소모품 + 쿨다운 | `Config/Tags/Ability.ini` |
| 플레이어 상태 + 차단 태그(`Block.*`) | `Config/Tags/State.ini` |
| 이동 속도 · 상태 효과 수치 | `GE_*` 에셋 |
| 입력 ↔ 어빌리티 매핑 | `AbilityInputBindings` (BP) |

> **TODO:** 스킬 강화(은신처)용 `CT_AbilityScaling` 커브 테이블이 아직 없다.
> `Config/DefaultGame.ini` 의 `GlobalCurveTableName` 이 주석 처리되어 있다.

---

## 5. 네트워크

| 값 | 클래스 | 복제 |
|---|---|---|
| `HeldActor` | `ABaseCharacter` | `ReplicatedUsing = OnRep_HeldActor` |
| `Gold` | `APlayerSessionState` | `ReplicatedUsing = OnRep_Gold` |
| `SelectedCharacterID` | `APlayerSessionState` | `Replicated` |
| `Health` `MovementSpeed` | `UBaseAttributeSet` | `DOREPLIFETIME_CONDITION_NOTIFY(..., REPNOTIFY_Always)` |
| `RecentlyThrownActor` | `ABaseCharacter` | **안 함** (서버 권위 로직 전용) |

**ASC 를 PlayerState 에 둔 이유** — 체포 → 관전 → 복귀 시 폰이 파괴돼도 스킬 강화 수치가
유지되어야 한다. 대가로 PlayerState 가 폰보다 늦게 도착할 수 있다
([02-Networking.md](../02-Networking.md) 8장).

---

## 6. 다른 시스템과의 접점

| 상대 | 방향 | 수단 |
|---|---|---|
| 물리 · 아이템 (김민준) | 양방향 | `ICarryable`, `SetHeldActor` |
| 소음 (지성인) | 이쪽 → | `FNoiseModifier` 등록 (고무창 신발, 발소리 위장) |
| 세션 (이지은) | ← 이쪽 | `APlayerSessionState` 통합 (아래 TODO) |
| UI | 이쪽 → | 스킬 쿨다운, 체력 · 스태미나 |

---

## 7. 디버깅

| 수단 | 비고 |
|---|---|
| `hh.Ability.Debug` | `1` 트레이스 / `2` 궤적까지 |
| `LogInteract` | 상호작용 진단 |

---

## 8. 알려진 제약과 TODO

> **TODO: PlayerState 통합.** `APlayerSessionState` 로 통합하기로 확정됐다.
> `AShelterPlayerState`(develop, 비어 있음)를 삭제하고 GameMode · GameState 의
> PlayerState 클래스를 교체할 것. 이지은과 함께 처리
> ([01-Architecture.md](../01-Architecture.md) 5장).

> **TODO: ASC 접근 경로 이중화.** `ABaseCharacter` 와 `APlayerSessionState` 가 둘 다
> `IAbilitySystemInterface` 를 구현 중이다.
> `ABaseCharacter::GetAbilitySystemComponent()` 가 PlayerState 의 ASC 를 돌려주도록 정리할 것.

> **TODO:** `Server_ApplyGameplayEffect` 에 `WithValidation` 이 없다
> ([05-Conventions.md](../05-Conventions.md) 3장).

> **TODO:** 현재 어빌리티 3종은 **역할 무관 공통**이다. 기획서 4-1장의 역할별 16 스킬은 미착수.

> **TODO:** `Content/Developers/Doom/Mesh/` 캐릭터 에셋의 출처와 라이선스가 문서화되어 있지 않다
> ([08-Plugins.md](../08-Plugins.md) 4장).

> **TODO:** `Content/Developers/Doom/Untitled.umap` 은 이름이 임시다.
