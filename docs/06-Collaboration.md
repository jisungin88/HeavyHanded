# 06. 협업 규칙 — 브랜치 · 커밋 · 담당 경계

> 기존 Notion "팀 컨벤션" 페이지의 1장(커밋 메시지)과 4-3·4-4(충돌 방지·Git 순서)를 여기로 옮겼다.
> **이 문서가 원본이다.**

---

## 1. 이 문서의 범위

**여기서 정하는 것** — 누가 무엇을 소유하는가, 브랜치, 커밋 메시지, Git 작업 순서, 에셋 충돌 방지, 남의 영역을 고칠 때의 절차.

**여기서 정하지 않는 것**

| 주제 | 문서 |
|---|---|
| 코드·에셋 네이밍, 폴더 구조 | [05-Conventions.md](05-Conventions.md) |
| 태그 파일에 무엇을 넣는가 | [04-DataManagement.md](04-DataManagement.md) 3장 |
| 왜 로직을 C++ 로 쓰는가 | [03-CppVsBlueprint.md](03-CppVsBlueprint.md) 6장 |

---

## 2. 담당 영역과 파일 소유권

> 2026-08-06 배정. 인원이나 범위가 바뀌면 여기를 고친다.

| 담당 | 영역 | git 계정 | 시스템 문서 |
|---|---|---|---|
| 지성인 | 소음 & 경계도 | `jisungin` | [systems/Noise-Alert.md](systems/Noise-Alert.md) |
| 이지은 | 세션 & 네트워크 | `LEEEJJJJ` | [systems/Session.md](systems/Session.md) |
| 전영배 | 플레이어 조작, 스킬 & 패시브 | `ybj0212` | [systems/Player-GAS.md](systems/Player-GAS.md) |
| 유정석 | AI | `badapple22` | [systems/Guard-AI.md](systems/Guard-AI.md) |
| 김민준 | 물리, 아이템 | `kimminjun020220-netizen` | [systems/Loot-Physics.md](systems/Loot-Physics.md) |
| 오유석 | 환경 방해 요소, 협력 장치 | — | (예정) |

### `Config/Tags/*.ini` 배정

태그 파일이 시스템별로 쪼개져 있는 이유가 이것이다 — **머지 충돌 격리.**
자기 파일에만 태그를 추가한다.

| 파일 | 담당 |
|---|---|
| `Noise.ini`, `Alert.ini` | 지성인 |
| `Guard.ini` | 유정석 |
| `Hazard.ini` | 오유석 |
| `Loot.ini`, `Equipment.ini` | 김민준 |
| `Role.ini`, `Ability.ini`, `State.ini` | 전영배 |
| `Phase.ini` | 이지은 |
| `Cue.ini`, `Event.ini` | 공용 — 각자 자기 것만 추가 |

### git 신원을 먼저 설정한다

각 머신에서 **처음 클론하기 전에** 한 번 확인한다.

```bash
git config --global user.name  "본인 GitHub 계정명"
git config --global user.email "GitHub 에 등록된 메일"
```

설정하지 않으면 `DESKTOP-XXXX\admin` 같은 머신 이름으로 커밋이 올라간다.
**지금 리포에 그렇게 올라간 커밋이 20개 있다.** 이력에서 누가 무엇을 했는지 추적할 수 없고,
GitHub 기여도에도 잡히지 않는다. 이미 올라간 것은 되돌리기 어려우니 **앞으로만이라도 맞춘다.**

---

## 3. 브랜치 전략

```
main                 릴리스. 현재는 초기 세팅 커밋 그대로 (아직 사용하지 않음)
└── develop          통합 기준. 모든 feature 가 여기로 모인다
    ├── feature/AI
    ├── feature/PlayerAndSkills
    ├── feature/physics
    └── feature/uiux
```

- **`develop` 이 실질적 기준선이다.** 작업은 `develop` 에서 따서 `develop` 으로 되돌린다.
- **`main` 은 아직 쓰지 않는다.** 플레이 가능한 빌드가 나오는 시점에 `develop` → `main` 을 한 번 올린다.
- 새 브랜치 이름은 **소문자**로 짓는다 — `feature/hazard`, `feature/equipment`.
  기존 브랜치(`feature/PlayerAndSkills` 등)는 그대로 둔다. 이름을 바꾸면 팀원 전원이 다시 받아야 해서
  얻는 것보다 잃는 것이 크다.

### develop 을 정기적으로 당겨온다

**feature 브랜치가 오래 떨어져 있을수록 병합 비용이 기하급수로 오른다.**
특히 `Config/*.ini` 와 `Source/.../Shared/` 는 여러 사람이 건드린다.

> **주 1회 이상 `develop` 을 자기 브랜치로 머지할 것.**
> 뒤처진 커밋 수는 `git rev-list --count 내브랜치..origin/develop` 로 확인한다.

### 병합이 끝난 브랜치는 지운다

`develop` 에 병합된 feature 브랜치는 원격에서 삭제한다.
살아 있으면 브랜치 목록에서 "아직 작업 중인 것"과 구별되지 않는다.

```bash
git push origin --delete feature/<이름>
git remote prune origin          # 각자 로컬에서
```

**삭제 전에 팀 채팅에 공지한다.** 로컬에 미푸시 커밋을 들고 있는 사람이 있을 수 있다.

> **TODO:** 병합이 끝났는데 남아 있는 브랜치가 4개 있다 —
> `feature/NoiseAndBoundary`, `feature/env`, `feature/network`, `feature/network_hold`.
> (`network` 와 `network_hold` 는 같은 커밋을 가리킨다.) 공지 후 정리할 것.

---

## 4. 커밋 메시지 규약

### 형식

```
type: 무엇을 했는지 한 줄

- 세부 변경 1
- 세부 변경 2
```

- **타입 뒤에 바로 콜론.** 콜론 앞에 공백을 넣지 않는다. (`test :` ✗ / `test:` ✓)
- **scope 는 쓰지 않는다.** `feat(loot):` 가 아니라 `feat:` 다.
  과거 로그에 scope 가 붙은 커밋이 섞여 있는데, 규약으로 정해진 적은 없다.
- 한 줄 요약은 **무엇을 했는지**를 적는다. 본문에 이유와 세부를 적는다.

### 타입

| 타입 | 설명 | 예시 |
|---|---|---|
| `feat` | 새로운 기능 추가 | `feat: 소음 전파 · 감쇄 서브시스템 구현` |
| `fix` | 버그 수정 | `fix: 결제 요청 시 네트워크 에러 수정` |
| `docs` | 문서 수정 | `docs: 기술설계서 데이터 관리 항목 추가` |
| `style` | 포맷팅 등 로직 변경 없는 수정 | `style: 변수명 오타 수정 및 줄바꿈 정리` |
| `refactor` | 기능 변경 없이 코드 구조 개선 | `refactor: 소음 · 경계 시스템 정리` |
| `test` | 테스트 코드 추가 · 수정 | `test: 스팸 필터 자동화 테스트 추가` |
| `chore` | 빌드, 설정, `.gitignore` 등 | `chore: .gitignore 에 LFS 규칙 추가` |

**이 일곱 개만 쓴다.** 로그에 섞여 있는 `merge:`, `WIP:`, `doc:` 은 규약에 없다.

- 작업 중간 저장도 타입을 붙인다 — `chore: 캐릭터 스킬 작업 중 저장`
- 병합 커밋은 **git 이 만들어 주는 메시지를 그대로 둔다.** 손으로 `merge:` 를 쓰지 않는다
- 자주 나는 오타 두 개 — `refactor`(`refector` ✗), `docs`(`doc` ✗)

### 작성 예시

```
fix: 캐릭터 점프 중 충돌 체크 오류 수정

- 공중 상태에서 두 번 점프되는 판정 버그 수정
- GroundCheck 레이캐스트 거리값 조정 (0.5f -> 0.2f)
```

### BP 를 깨뜨리는 변경은 커밋 메시지에 적는다

`BlueprintCallable` 함수의 이름이나 시그니처를 바꾸면 BP 노드가 끊긴다.
**어느 에셋을 누가 고쳐야 하는지 본문에 명시한다.**

```
refactor: 채팅 RPC 이름을 컨벤션에 맞게 변경

- ServerSendChatMessage -> Server_SendChatMessage
- ClientReceiveChatMessage -> Client_ReceiveChatMessage
- [이지은] WBP_ChatBox 의 노드 재연결 필요
```

---

## 5. Git 작업 순서

`.uasset` 은 에디터가 메모리에 들고 있다. **에디터가 켜진 채로 `git pull` 하면
파일이 잠겨 있거나, 메모리에 남아 있던 구버전이 나중에 저장되면서 받아온 내용을 덮어쓴다.**

```
1. 에디터에서 전체 저장 (Ctrl + Shift + S)
2. 에디터를 닫는다        ← pull 전에 반드시
3. git pull
4. 에디터를 다시 열고 작업
5. 커밋 전 다시 전체 저장 후 에디터를 닫는다
6. 커밋 후 푸시
```

### Git LFS

**`git lfs install` 은 clone 전에 각자 1회 필수다.**

빠뜨린 사람이 올린 에셋만 LFS 를 타지 않고 실제 바이너리로 들어간다.
나중에 골라내려면 **히스토리 재작성**이 필요하다 — 팀 전원이 다시 클론해야 한다는 뜻이다.

LFS 대상은 `.gitattributes` 에 정의되어 있다 — `.uasset` `.umap` 과 메시·텍스처·오디오·영상.

### 파일 락은 아직 쓰지 않는다

`.gitattributes` 에 `lockable` 설정이 주석으로 준비되어 있지만 **켜지 않는다.**
락을 모르는 팀원이 "에디터에서 저장이 안 돼요" 상태에 빠지는 비용이 크다.

지금은 6장의 **채팅 공지 방식**으로 운영한다.
공지로 막지 못한 에셋 충돌 사고가 실제로 나면 그때 전원에게 절차를 공유하고 켠다.

---

## 6. 에셋 충돌 방지

`.uasset` / `.umap` 은 바이너리라 **Git 이 병합할 수 없다.**
코드 충돌은 해결할 수 있지만, **에셋 충돌은 한쪽 작업이 그대로 소실된다.**

### 레벨은 두 명이 동시에 열지 않는다

작업 시작 전 팀 채팅에 남긴다.

```
"Mansion 작업 진행합니다."
...
"Mansion 작업 완료했습니다."
```

### 같은 블루프린트를 두 사람이 수정하지 않는다

시스템별 담당자(2장)를 기준으로 한다. 남의 영역을 고쳐야 하면 7장 절차를 따른다.

### 로직은 C++ 로 쓴다

**충돌 비용이 비교할 수 없이 낮기 때문이다.** C++ 은 diff 와 병합이 되지만 BP 는 안 된다.
BP 는 C++ 클래스를 상속해 **값만 지정하는 껍데기**로 유지한다.
자세한 경계는 [03-CppVsBlueprint.md](03-CppVsBlueprint.md).

---

## 7. 경계를 넘는 작업

**담당 경계는 머지 충돌을 줄이기 위한 것이지 금지선이 아니다.**
남의 영역을 고쳐야 하는 상황은 실제로 생긴다.

### 절차

1. **담당자에게 먼저 알린다.** 그 사람이 지금 같은 파일을 열고 있을 수 있다
2. 고친다
3. **커밋 메시지 본문에 담당자를 표기한다** — `[유정석] BT 노드 시그니처 변경`

### 먼저 합의가 필요한 경우

| 상황 | 이유 |
|---|---|
| **여러 영역에 걸치는 새 인터페이스** | 누가 소유할지 정하지 않으면 나중에 양쪽이 각자 고친다 |
| **`Source/.../Shared/` 에 파일 추가** | 전원이 쓰는 코드다. [01-Architecture.md](01-Architecture.md) 규칙 4 참조 |
| **`Config/Default*.ini` 수정** | 전원의 프로젝트 설정이 바뀐다 |
| **`BlueprintCallable` 시그니처 변경** | 남의 BP 가 끊긴다 (4장) |

### 먼저 물어보지 않아도 되는 경우

- 자기 영역 안의 작업
- 오타 · 인코딩 · 포맷 수정
- 명백한 버그 수정 (단, 커밋 메시지에 담당자 표기)

---

## 8. 이 문서를 고쳐야 할 때

- 담당이 바뀌면 2장 표와 `Config/Tags` 배정을 같이 고친다
- 새 태그 파일이 생기면 2장 배정표에 추가한다
- **파일 락을 켜기로 하면** 5장을 고치고 `.gitattributes` 주석을 해제한다
- 규약과 실제 커밋이 어긋나기 시작하면, 규약을 지키게 하든 규약을 바꾸든
  **한쪽으로 정한다.** 어긋난 채로 두면 아무도 안 지키게 된다
