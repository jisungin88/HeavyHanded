# HeavyHanded (SNEAKERS) — 팀 컨벤션

> 이 파일은 Notion "문서" 데이터베이스의 01~08 번호 문서를 요약한 것이다.
> Systems/* (소음·경계도, AI, 물리, 플레이어·GAS, UI, 세션 등 시스템별 상세 설계)는 여기 포함하지 않는다 — 필요할 때 사용자가 링크한다.
> 원본: https://app.notion.com/p/3b2aee16de23803fbeb2fea9538ee172
> **상세 내용이 필요하면 Notion 원본을 먼저 확인할 것.** 여기는 매 세션 반복 설명을 피하기 위한 압축본이다.

## 프로젝트 개요

- UE5.4, Listen Server, 2~4인 협동 잠입 게임 (SNEAKERS), 6인 팀
- 런타임 모듈은 `HeavyHanded` 1개. 시스템 경계는 모듈이 아니라 **폴더와 클래스 소유권**으로 나뉜다
- 세션: `OnlineSubsystemNull` (LAN + 직접 IP). Steam은 추후
- **git 계정: `badapple22` = 유정석, 담당 영역: AI (경비 순찰·조사·추격), 브랜치 `feature/AI`**
  - 이 저장소에서 작업할 때는 기본적으로 이 사람 관점(AI 시스템, `Config/Tags/Guard.ini`)으로 판단한다

## ⚠️ 절대 규칙 (예외 없음)

- UObject 포인터 멤버에는 예외 없이 `UPROPERTY()`. 없으면 GC가 회수 → 몇 분 뒤 관계없는 코드에서 크래시 (재현 어렵고 추적에 며칠 걸림)
- 게임 상태를 바꾸는 코드는 반드시 서버에서만 (`HasAuthority()` / `HasServerAuthority()`)
- 복제 변수는 반드시 `GetLifetimeReplicatedProps()`에 등록 (빠뜨려도 컴파일 에러 없음, 호스트는 정상, 클라만 갱신 안 됨)
- `.uasset`/`.umap`은 Git이 병합 못 함 — 같은 레벨/블루프린트 동시에 열지 않는다
- 이 게임은 **Listen Server**다. 호스트는 서버+플레이어를 겸해서 **호스트 창에서만 테스트하면 거의 모든 네트워크 버그가 숨는다.** 반드시 PIE 2인 이상(Net Mode: Play As Listen Server), 클라이언트 창에서 확인할 것

---

## 01. 시스템 아키텍처

### 레이어 (참조 방향 규칙)
1. **컴포넌트는 자기 소유 액터의 구체 타입을 모른다** — 전방 선언만, GameState 서브클래스 include 금지
2. **월드 서브시스템은 구체 액터 타입을 모른다** — 인터페이스(`INoiseListener` 등)로만 접근
3. **권위 판정은 `Shared/NetAuthority.h` 하나만 쓴다.** 컴포넌트/서브시스템에서 `GetOwnerRole()` 직접 사용 금지 (클라 로컬 스폰 비복제 액터가 게이트를 뚫는 함정 있음). 액터는 `AActor::HasAuthority()` 그대로 사용
4. **공유 계산식은 한 곳에만** (`Shared/Gauge01.h` 등) — 복사본이 갈라지면 테스트가 프로덕션이 아니라 자기 복사본을 검증하게 됨
5. **UI는 시스템 상태를 읽고 구독만 한다.** 쓰지 않는다

### 시스템 인벤토리 (담당 / 브랜치 / 핵심 클래스)
| 시스템 | 담당 | 브랜치 | 핵심 클래스 |
|---|---|---|---|
| 소음·경계도 | 지성인 | develop | `UNoiseSubsystem` `UAlertComponent` `UPerceptionMeterComponent` `INoiseListener` |
| 노획물 물리 | 김민준 | feature/physics | `ALootBase` `ULootDurabilityComponent` `ICarryable` |
| 플레이어·GAS | 전영배 | feature/PlayerAndSkills | `ABaseCharacter` `APlayerSessionState` `UBaseGameplayAbility` |
| **경비 AI** | **유정석** | **feature/AI** | `AGuardAIController` `AGuardCharacter` + BT 노드 7종 |
| UI | (미정) | feature/uiux | `UAlertGaugeWidget` `UPerceptionMeterWidget` |
| 세션·로비 | 이지은 | develop | `ATitlePlayerController` `AShelterGameState` |

### 시스템 간 통신 수단 (5종, 고정)
- **C++ 멀티캐스트** (`DECLARE_MULTICAST_DELEGATE_*`) — 서버 권위 판정 경로. **기본값**, BP 노출 안 함
- **Dynamic 멀티캐스트** (`BlueprintAssignable`) — 판정이 **끝난 뒤** 결과 통지
- **인터페이스** — 호출자가 구체 타입을 몰라야 할 때
- **GameplayTag/GameplayEvent** — 어빌리티 트리거
- **DataTable/DeveloperSettings** — 밸런싱 수치
- 새 수단을 늘리는 건 여섯 번째를 도입할 때뿐. 새 델리게이트는 6장이 아니라 각 시스템 문서에 적는다

---

## 02. 네트워크 & 리플리케이션

### 권위 판정 — 대상별로 다르다
| 대상 | 쓸 것 |
|---|---|
| `UActorComponent` | `HasServerAuthority(this)` |
| 서브시스템 등 소유 액터 없는 곳 | `HasServerAuthority(GetWorld())` |
| `AActor` | `AActor::HasAuthority()` |

- **여러 사람이 호출하는 API는 게이트를 호출부가 아니라 API 안에 둔다** (예: `UNoiseSubsystem`은 클라에서도 인스턴스가 생기지만 발행 API가 조용히 무시됨)
- Server RPC에는 `WithValidation`을 붙인다. `_Validate`는 **악의적 입력만** 막는다 (검증 실패 = 해당 클라 접속 끊김). "빈 문자열" 같은 정상 범위 오입력은 `_Implementation`에서 조용히 무시
- **NetMulticast는 연출 전용.** 상태는 항상 `Replicated` + `OnRep`으로 동기화한다 (Multicast는 늦게 접속한 클라가 못 받음). 연출 Multicast는 `Unreliable`
- RPC 함수명에는 `Server_` / `Client_` / `Multicast_` 접두사

### OnRep 재실행 패턴 (시그니처 패턴, 반드시 지킬 것)
- Attach/물리토글/콜리전 변경은 **복제되지 않는 로컬 호출.** 복제되는 건 "누가 들고 있는가" 라는 사실 하나뿐
- **서버 경로와 OnRep이 반드시 같은 함수 하나만 호출한다** (예: `ApplyCarryState()`) — 서버·클라에 각각 코드를 쓰면 반드시 어긋나고, 로그에 안 남는 형태(클라에서만 안 붙음)로 나타남
- 순서 중요: 물리 시뮬레이션 중인 컴포넌트는 웰드 없이 부착 불가 → **반드시 물리 먼저 끄고 부착**

### 대역폭
- 0~1 게이지는 `Shared/Gauge01.h`로 `uint8` 양자화해서 복제. **양자화된 값으로 판정 금지** — 단계 판정은 항상 원본 float
- 파생 가능해 보여도 히스테리시스 등 상태 의존성이 있으면 별도 복제 (`AlertLevel`이 그 예)
- 물리 액터는 `PredictiveInterpolation` 모드 (`SetPhysicsReplicationMode`, `BeginPlay`에서). 클라는 예측하지 않고 서버 스냅샷으로 보간만

### 증상 → 원인 역인덱스 (자주 나는 사고)
- 호스트는 되는데 클라만 안 됨 → 클라 창에서 테스트 안 함
- 클라에서만 물건이 안 붙음 → 서버/OnRep이 다른 코드 경로
- 물건이 발밑에서 튕김 → 물리 끄기 전에 부착 (순서 위반)
- 값이 클라에 안 옴 → `DOREPLIFETIME` 등록 누락
- 늦게 접속한 사람만 상태 다름 → 상태를 `NetMulticast`로 바꿈
- HUD 게이지가 서버와 미세하게 어긋남 → 양자화 식을 복사해서 씀

---

## 03. C++ / 블루프린트 역할 분담

**한 문장: C++는 "무슨 일이 일어났는가", BP는 "그게 어떻게 보이는가".**

판정 질문 (새로 BP에 열까 고민될 때):
1. 틀리면 플레이어마다 다른 결과가 나오는가 → C++ (서버 권위는 예외 없이 C++)
2. 없어도 게임이 성립하는가 (연출) → BP
3. 아티스트/기획자가 반복해서 만질 값인가 → BP 노출
4. 매 프레임/매 충돌마다 도는가 → C++ (`BlueprintNativeEvent`는 `ProcessEvent`를 타서 비용 있음, 물리 낙하 1회에 `OnHit` 5~15회 발생)

C++→BP 장치 선택:
- **`BlueprintImplementableEvent`** — 연출 훅 (가장 많이 씀). 판정 후 호출, BP는 그리기만. **BP 훅에서 게임 상태를 바꾸지 말 것** (클라에서도 돌기 때문)
- **`BlueprintAssignable`** — 구독자가 판정에 참여할 수 없는, 판정이 끝난 결과만
- **`BlueprintNativeEvent`** — BP가 동작 자체를 바꿔야 할 때만 (비용 있음)

기타 규칙:
- `BlueprintPure`는 부수효과 없는 조회에만 (연결마다 재실행됨, 비싼 계산을 pure로 열면 한 프레임에 여러 번 돎)
- BP가 호출 중인 `UFUNCTION` 이름/시그니처를 바꾸면 BP 노드가 끊긴다 — 커밋 메시지에 어느 BP를 고쳐야 하는지 적을 것
- **데이터는 BP, 로직은 C++** — 진짜 이유는 병합 비용 (C++는 diff/병합 가능, BP는 불가). `USTRUCT`로 모양+기본값만 C++에서, BP 서브클래스가 실제 값. `BlueprintReadOnly`가 기본
- 순수 BP 에셋(C++ 대응 클래스 없음): `GameplayEffect`, `InputAction`/`IMC`, `AnimBlueprint`류, `WBP_*`, `Material`/`Niagara`, `GameplayCue Notify`. **BehaviorTree는 반씩** — 트리 구조는 에셋, 노드(`BTTask_*`/`BTDecorator_*`/`BTService_*`)는 전부 C++

---

## 04. 데이터 구조 및 관리

데이터가 사는 네 곳 — **이름이면 GameplayTag, 수치는 아래 셋**:
| 저장소 | 무엇 |
|---|---|
| GameplayTag (`Config/Tags/*.ini`) | 식별자 (이름) |
| DataTable (`Content/HeavyHanded/Data/DataTables/`) | 같은 모양의 데이터가 N개 |
| DeveloperSettings (Project Settings) | 전역 단일 값 |
| BP 프로퍼티 | 개체별 값 |

- **태그에 수치를 넣지 않는다.** `DefaultGameplayTags.ini`에 직접 추가 금지, 시스템별 `Config/Tags/<System>.ini`에
- **`+GameplayTagList` 금지** — `Tags/*.ini`는 config hierarchy가 아니라 개별 파일로 직접 읽힘. `+` 붙이면 키 이름이 그대로 저장되어 태그 0개가 됨 (경고 없음)
- `DT_NoiseProfiles`의 `RowName`은 태그 이름과 같아야 하며, 없으면 부모 태그로 거슬러 올라간다 (`Noise.Loot.Throw` → `Noise.Loot` → `Noise`)
- `UDeveloperSettings::Get()`은 CDO라 **null 체크 불필요** (`GetDefault<T>()`는 모듈 로드 시점에 이미 존재)
- 수치를 하드코딩하지 않는다 — Settings가 유일한 진리원. 런타임 부착 컴포넌트(`UAlertComponent` 등)는 디테일 패널에 안 뜨므로 수치는 반드시 Settings로
- 에셋 참조는 `TSoftObjectPtr` 기본 (하드 참조는 모듈 로드 시점에 전부 로드되어 에디터/쿠킹 느려짐), 로드 후 캐시해서 붙들어 둔다

---

## 05. 코딩 컨벤션 & 폴더 구조

### 명명
- 파스칼케이스. bool은 `b` 접두사 (`bIsDead`)
- 클래스 접두사: `A`(Actor) `U`(UObject) `F`(구조체) `E`(enum) `I`(인터페이스) `T`(템플릿) — UHT가 검사함
- 파일명은 접두사 뺀 클래스명 (`ALootBase` → `LootBase.h/.cpp`)
- enum은 `enum class ... : uint8`
- RPC는 `Server_`/`Client_`/`Multicast_` 접두사

### UPROPERTY/UFUNCTION
- `Category` 필수 (계층은 `|`로: `"Loot|Throw"`)
- `BlueprintReadOnly` 기본, `BlueprintReadWrite`는 런타임 변경이 정말 필요할 때만
- 값 제약은 메타로 (`ClampMin`/`ClampMax`/`Units`)

### 헤더
- `.h`에서 `#include` 최소화, 전방 선언 우선. `.cpp`에서 실제 include
- 전방 선언 불가능(값 타입 구조체, 상속 부모, UPROPERTY 노출 enum)한 경우 왜 include했는지 한 줄 남긴다

### 포인터/GC
- UObject 포인터 멤버는 예외 없이 `UPROPERTY()`, 원시 포인터 대신 `TObjectPtr<T>`
- 사용 전 `IsValid()` 검사 (다른 플레이어/노획물 참조 시 필수)
- 큐/맵에 잠시 보관되는 참조는 `TWeakObjectPtr<T>`. **이걸 담은 구조체는 BP에 노출 금지** (수명 관리 깨짐)

### 주석
- public 클래스/함수/프로퍼티엔 `/** ... */` 한 줄 (헤더만 읽고 써야 함)
- "왜"까지 적어야 하는 경우: 순서가 중요한 곳, 직관과 어긋나는 값/선택, 실측으로 정한 임계값

### 인코딩
- `.h/.cpp/.inl/.cs`: 탭(폭4), **UTF-8 with BOM**, LF — BOM 없으면 MSVC가 CP949로 읽어 한글 주석이 C4819를 내거나 다음 줄을 통째로 주석으로 먹는 사고 발생
- `.ini`: 공백4, UTF-8, LF

### 폴더
- 소스: `Public/`·`Private/`이 동일 하위 구조 (`AI/ Alert/ Character/ Core/ Data/ Equipment/ Hazards/ Interaction/ Interfaces/ Loot/ Noise/ Shared/ UI/`)
- `Shared/`는 **여러 시스템이 같은 식을 써야 하는 것만** (잡동사니 폴더 아님)
- 콘텐츠: 우리가 만드는 에셋은 **`Content/HeavyHanded/` 하위에만.** 개인 실험물은 `Content/Developers/<이름>/`에서 작업 후 정식 폴더로 이동
- **폴더 이동/이름 변경은 반드시 언리얼 에디터 안에서** (윈도우 탐색기에서 하면 참조 다 깨짐)

### 에셋 접두사
`BP_` `WBP_` `M_` `MI_` `DT_` `DA_` `BB_/BT_` `SM_` `SK_` `T_` `S_` `SC_` `NS_` `IA_/IMC_` `GA_` `GE_`

---

## 06. 협업 규칙

### 브랜치
```
main                 릴리스 (아직 미사용)
└── develop          통합 기준
    ├── feature/AI              ← 유정석
    ├── feature/PlayerAndSkills ← 전영배
    ├── feature/physics         ← 김민준
    └── feature/uiux
```
- **주 1회 이상 `develop`을 자기 브랜치로 머지할 것** (뒤처질수록 병합 비용 기하급수)
- 새 브랜치명은 소문자로

### 커밋 메시지
```
type: 무엇을 했는지 한 줄

- 세부 변경 1
- 세부 변경 2
```
- 타입 뒤 바로 콜론, 공백 없음 (`fix:` O / `fix :` X)
- scope 쓰지 않음 (`feat(loot):` X, `feat:` O)
- 타입은 이 7개만: `feat` `fix` `docs` `style` `refactor` `test` `chore`
- 병합 커밋은 git이 만든 메시지 그대로 둔다
- `BlueprintCallable` 함수 이름/시그니처 변경 시 커밋 메시지에 어느 BP를 고쳐야 하는지 적는다 (`[담당자] WBP_X의 노드 재연결 필요`)

### Git 작업 순서 (에디터가 켜진 채 pull 금지 — 메모리의 구버전이 덮어씀)
```
1. 에디터에서 전체 저장 (Ctrl+Shift+S)
2. 에디터 닫기        ← pull 전 필수
3. git pull
4. 에디터 다시 열고 작업
5. 커밋 전 다시 전체 저장 후 에디터 닫기
6. 커밋 후 푸시
```
- `git lfs install`은 clone 전 각자 1회 필수 (빠뜨리면 실제 바이너리가 커밋되어 히스토리 재작성 필요)

### 에셋 충돌 방지
- 레벨(.umap)은 두 명이 동시에 열지 않는다 — 작업 전/후 팀 채팅에 공지
- 같은 블루프린트를 두 사람이 수정하지 않는다 (담당자 기준)
- 로직은 C++로 (충돌 비용이 비교할 수 없이 낮음). BP는 값만 지정하는 껍데기로 유지

### 남의 영역을 고칠 때
1. 담당자에게 먼저 알린다
2. 고친다
3. 커밋 메시지에 담당자 표기
- **먼저 합의 필요**: 여러 영역에 걸친 새 인터페이스, `Shared/`에 파일 추가, `Config/Default*.ini` 수정, `BlueprintCallable` 시그니처 변경
- **안 물어봐도 됨**: 자기 영역, 오타/인코딩/포맷, 명백한 버그 수정(담당자 표기는 할 것)

### Config/Tags/*.ini 담당
`Noise.ini`/`Alert.ini`=지성인, `Guard.ini`=**유정석**, `Hazard.ini`=오유석, `Loot.ini`/`Equipment.ini`=김민준, `Role.ini`/`Ability.ini`/`State.ini`=전영배, `Phase.ini`=이지은, `Cue.ini`/`Event.ini`=공용

---

## 07. 테스트·디버깅

- **이 게임의 버그는 크래시가 아니라 침묵으로 온다** (소음 안 남 → "경비가 못 듣는다", 복제 빠짐 → "클라에서만 안 됨", BT 실패 → "경비가 가만히 서 있음"). 컴파일 에러도 로그도 없다
- **모든 기능은 처음부터 멀티 환경에서 테스트** (PIE 2인 이상, Run Under One Process 해제, Net Mode: Listen Server). 싱글로 먼저 만들고 나중에 복제 붙이는 방식은 대부분 전면 재작성으로 끝남
- Automation Test 대상: 월드 없이 검증 가능한 순수 함수/상태 머신, 밸런싱으로 계속 바뀔 자리, 실패가 침묵하는 것. 이름 규칙: `HeavyHanded.<시스템>.<대상>.<검증내용>` (검증 내용은 "무엇이 참이어야 하는가"로)
- 테스트는 **프로덕션 코드를 직접 검증**해야 한다 (private이라 규칙을 테스트에 복제하면 자기 복사본만 검증하게 됨 — 회귀 못 잡음)
- CVar 이름 규칙: `hh.<시스템>.<대상>`. 치트는 `#if !UE_BUILD_SHIPPING` + `ECVF_Cheat`로 감싼다
- 로그: `.cpp` 안에서만 쓰면 `DEFINE_LOG_CATEGORY_STATIC`. 여러 파일 공유 시에만 헤더에 `DECLARE_LOG_CATEGORY_EXTERN` (`LogGuardAI`가 이 경우 — BT 노드 7개가 공유)
- **조용히 실패하는 지점(early return)마다 로그를 남긴다** — 권위 체크 탈락, null 참조, 데이터 못 찾음
- 반복 경로(물리 충돌 등)에서는 한 번만 찍는다 (태그당 1회 등)
- 디버그 드로잉은 `#if ENABLE_DRAW_DEBUG`로 감싼다 (런타임 if만으로 막으면 쉬핑에 코드가 남음)
- 테스트 액터/맵은 프로덕션 클래스에 섞지 않는다. AI 전용: `GuardTest` 맵 (`Content/HeavyHanded/Maps/Test/`)

---

## 08. 외부 플러그인 & 에셋

- 전부 엔진 내장 플러그인, 서드파티 0개. 에디터 전용 플러그인은 `TargetAllowList: ["Editor"]`로 런타임 빌드에서 제외
- 도입하지 않기로 한 것 (재제안 전에 이유가 아직 유효한지 확인): CommonUI, Chaos Geometry Collection, `bEnablePhysicsPrediction`, `OnlineSubsystemSteam`(추후), LFS 파일 락(보류)
- 외부 에셋 반입 규칙: 원래 폴더 구조 유지 (`Content/HeavyHanded/`로 옮기지 않음), LFS 대상 확인, 라이선스/출처 기록, 대용량은 팀 공지

---

## 이 파일을 고쳐야 할 때

- Notion 원본(01~08)이 바뀌면 이 요약도 같이 고친다 (원본이 유일한 진리원)
- 여기 없는 세부사항이 필요하면 위 각 섹션의 Notion 링크를 먼저 확인
- 특정 시스템(소음·경계도, AI, 물리, 플레이어·GAS, UI, 세션)의 내부 설계·수치·TODO는 이 파일에 없다. 필요하면 사용자에게 해당 Systems/* 문서 링크를 요청할 것
