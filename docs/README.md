# Sneakers — 기술설계서

기획명 **Sneakers**(가칭) / 리포지토리 **HeavyHanded**.
2~4인 온라인 협동 물리 운반 게임. Unreal Engine 5.4, Listen Server.

기획 문서는 별도로 관리된다. 이 문서 묶음은 **기획을 어떻게 구현할 것인가**만 다룬다.

> 기존 Notion "팀 컨벤션" 페이지는 여기로 흡수했다.
> C++ 컨벤션·에셋 규칙은 [05-Conventions.md](05-Conventions.md), 네트워크 규칙은 [02-Networking.md](02-Networking.md),
> 커밋·협업 규칙은 [06-Collaboration.md](06-Collaboration.md) 에 있다.
> **규칙을 고칠 때는 이 리포의 문서를 고친다.** Notion 은 입구 링크로만 남긴다.

---

## 이 문서 묶음을 읽는 법

문서는 두 종류로 나뉜다.

- **공통 규범** (`01`~`08`) — 전원이 지켜야 하는 규칙. 잘 바뀌지 않는다. 바꾸려면 팀 논의가 필요하다.
- **시스템별 설계** (`systems/`) — 담당자가 자기 시스템을 기술한다. 자주 바뀌며, 담당자가 직접 갱신한다.

담당자별로 파일을 갈라둔 이유는 `Config/Tags/*.ini` 를 담당별로 쪼개둔 것과 같다 —
6명이 한 파일을 동시에 고치면 머지 충돌이 계속 난다.

### 새로 합류했다면

1. `01-Architecture.md` — 전체 조감도
2. `02-Networking.md` — 서버 권위 원칙 (이걸 모르면 어떤 코드도 못 짠다)
3. `05-Conventions.md` — 폴더 구조, 네이밍, 인코딩
4. 자기가 맡을 `systems/` 문서

---

## 공통 규범

| 문서 | 다루는 것 | 상태 |
|---|---|---|
| [01-Architecture.md](01-Architecture.md) | 레이어 구조, 클래스 계층, 시스템 간 통신 규약, 소유권·생명주기, 레벨 구성 | 작성됨 |
| [02-Networking.md](02-Networking.md) | Listen Server 구조, 서버 권위 원칙, RPC/OnRep/Multicast 사용 규칙, 물리 리플리케이션, GAS 복제 정책, 증상 역인덱스 | 작성됨 |
| [03-CppVsBlueprint.md](03-CppVsBlueprint.md) | 무엇을 C++ 로 짜고 무엇을 BP 에 맡기는가. 경계를 넘기는 장치 5종, 차단 목록, 유형별 작업 가이드 | 작성됨 |
| [04-DataManagement.md](04-DataManagement.md) | 데이터가 사는 네 곳, GameplayTag 체계, DataTable 규약, Project Settings, 에셋 참조, 배치 판정표 | 작성됨 |
| [05-Conventions.md](05-Conventions.md) | **팀 컨벤션 원본.** 네이밍, 매크로, 헤더 작성, 포인터 안전, 주석, 인코딩, 폴더 구조 | 작성됨 |
| [06-Collaboration.md](06-Collaboration.md) | 담당 영역과 파일 소유권, 브랜치 전략, 커밋 메시지 규약, Git 작업 순서, 에셋 충돌 방지 | 작성됨 |
| [07-Testing.md](07-Testing.md) | 멀티플레이 테스트 기준, Automation Test 6종, 콘솔 변수·치트, 로그 카테고리, 디버그 드로잉, 테스트 맵 | 작성됨 |
| [08-Plugins.md](08-Plugins.md) | 활성 플러그인, **도입하지 않기로 한 결정과 재검토 조건**, 외부 에셋 반입 규칙 | 작성됨 |

## 시스템별 설계

| 문서 | 시스템 | 담당 | 코드 위치 | 상태 |
|---|---|---|---|---|
| [systems/Noise-Alert.md](systems/Noise-Alert.md) | 소음 전파 · 경계도 · 경비 인지 게이지 | 지성인 | `develop` | **작성 완료** |
| [systems/Loot-Physics.md](systems/Loot-Physics.md) | 노획물 물리 · 운반 · 파손 | 김민준 | `feature/physics` | 골격 |
| [systems/Player-GAS.md](systems/Player-GAS.md) | 플레이어 조작 · GAS · 어빌리티 | 전영배 | `feature/PlayerAndSkills` | 골격 |
| [systems/Guard-AI.md](systems/Guard-AI.md) | 경비 AI · 순찰 · 조사 · 추격 | 유정석 | `feature/AI` | 골격 |
| [systems/UI.md](systems/UI.md) | HUD · 위젯 · 디자인 토큰 | **미정** | `feature/uiux` | 골격 |
| [systems/Session.md](systems/Session.md) | 세션 · 로비 · 채팅 | 이지은 | `develop` | 골격 |

**골격** = 클래스 목록 · 파일 경로 · 접점 등 확인된 사실만 채워져 있고, 설명은 `TODO` 로 비어 있는 상태.
[systems/Noise-Alert.md](systems/Noise-Alert.md) 가 작성 형식의 기준이다.

> `feature/*` 에 있는 시스템의 문서는 **아직 `develop` 에 없는 파일을 가리킨다.**
> 각 문서 머리말에 코드가 있는 브랜치를 표시해 두었다.

---

## 문서 작성 규칙

- **커밋되어 확인 가능한 것만 쓴다.** 아직 구현되지 않은 영역을 "이렇게 설계되어 있다"고 쓰지 않는다.
  남겨야 할 것은 `TODO:` 로 표시하고, 무엇이 막고 있는지 한 줄 적는다.
- **수치는 여기 적지 않는다.** 밸런싱 수치는 `DT_NoiseProfiles` 와 Project Settings 가 원본이다.
  문서에 수치를 복사하면 반드시 어긋난다. 어디를 보면 되는지만 적는다.
- **결정에는 근거를 남긴다.** 이 팀의 코드는 이미 그렇게 쓰여 있다
  (`FLootPhysicsData::CarrierVelocityInfluence`, `UAlertComponent::ReplicatedGauge` 주석 참조).
  근거가 없으면 나중에 누군가 되돌린다.
- **코드 경로는 파일명으로 적는다.** 줄 번호는 금방 어긋난다.
