# 세션 · 로비 · 채팅

| | |
|---|---|
| **담당** | 이지은 |
| **코드 위치** | `develop` (병합 완료) |
| **소스** | `Public|Private/Core/` |
| **태그** | `Config/Tags/Phase.ini` |
| **플러그인** | `OnlineSubsystem`, `OnlineSubsystemNull` |
| **맵** | `LV_0_LoginTitle`, `LV_1_Shelter` |

> **이 문서는 골격이다.** 표에 확인된 사실만 채워져 있고, 설명은 담당자가 채운다.
> 작성 형식은 [Noise-Alert.md](Noise-Alert.md) 를 참고할 것.

---

## 1. 이 시스템이 하는 일

기획서 2장 — 코어 루프의 진입부. 방 생성 · 검색 · 참가, 로비 대기, 은신처 허브.

> **TODO:** 한 문단으로.

---

## 2. 클래스 구성

| 클래스 | 부모 | 역할 |
|---|---|---|
| `ATitleGameMode` | `AGameMode` | 타이틀 |
| `ATitlePlayerController` | `APlayerController` | **세션 생성 · 검색 · 참가** |
| `AShelterGameMode` | `AGameMode` | 은신처. `PostLogin` / `Logout` |
| `AShelterGameState` | `AGameState` | 로비 인원 집계 |
| `AShelterPlayerController` | `APlayerController` | 채팅 RPC |
| `AShelterPlayerState` | `APlayerState` | **비어 있음 — 삭제 예정** (아래 TODO) |

### 타입

`FRoomListData` — 방 이름, 현재/최대 인원, `FOnlineSessionSearchResult`

### 델리게이트 (BP 가 구독)

| 델리게이트 | 클래스 |
|---|---|
| `FOnSessionCreated` | `ATitlePlayerController` |
| `FOnRoomListUpdated` | `ATitlePlayerController` |
| `FOnLobbyPlayerCountChanged` | `AShelterGameState` |
| `FOnChatMessageReceived` | `AShelterPlayerController` |

### BP 에셋

`WBP_LoginMenu`, `WBP_ServerMenu`, `WBP_ServerButton`, `WBP_ChatBox`,
`WBP_Loading`, `WBP_PlayerNickname`, `WBP_ShelterParty`
(`Content/Developers/Lee/0Server/Widget/`)

---

## 3. 핵심 흐름

> **TODO:** 다음을 각각. 특히 비동기 콜백 순서와 실패 경로를 명시할 것.
> - 방 생성 (`TitleCreateSession` → `TitleOnCreateSessionComplete` → `TitleOnStartSessionComplete`)
> - 방 검색 (`TitleFindSessions` → `TitleOnFindSessionsComplete`)
> - 참가 (`TitleJoinSession` → `TitleOnJoinSessionComplete`)
> - 로비 → 은신처 레벨 전환
> - 채팅 (`Server_SendChatMessage` → 전원 `Client_ReceiveChatMessage`)

---

## 4. 데이터와 수치

| 무엇 | 어디 |
|---|---|
| 게임 진행 단계 · 장소 태그 | `Config/Tags/Phase.ini` |
| 세션 서브시스템 | `Config/DefaultEngine.ini` `[OnlineSubsystem] DefaultPlatformService=Null` |
| 채팅 최대 길이 | `AShelterPlayerController::MaxChatMessageLength` (200) |

---

## 5. 네트워크

| RPC | 종류 | 비고 |
|---|---|---|
| `Server_SendChatMessage` | `Server, Reliable, WithValidation` | 200자 초과는 `_Validate` 에서 거부 |
| `Client_ReceiveChatMessage` | `Client, Reliable` | |

**`_Validate` 실패는 엔진이 해당 클라이언트의 접속을 끊는 동작이다.**
빈 문자열처럼 정상 범위의 잘못된 입력은 `_Implementation` 에서 조용히 무시한다
([02-Networking.md](../02-Networking.md) 4장).

`APlayerSessionState` 의 복제 값은 [Player-GAS.md](Player-GAS.md) 5장 참조.

---

## 6. 다른 시스템과의 접점

| 상대 | 방향 | 수단 |
|---|---|---|
| 플레이어 · GAS (전영배) | 양방향 | PlayerState 통합 (아래 TODO) |
| UI | 이쪽 → | 세션 · 채팅 · 로비 인원 델리게이트 |
| 전 시스템 | 이쪽 → | `Phase.*` 태그로 게임 단계 방송 (미착수) |

---

## 7. 디버깅

> **TODO:** 세션 관련 진단 수단이 없다. 전용 로그 카테고리(`LogSession` 등)를 만들 것.
> 세션 실패는 조용히 실패하는 대표적인 경로다 ([07-Testing.md](../07-Testing.md) 5장).

---

## 8. 알려진 제약과 TODO

> **TODO: PlayerState 통합.** `APlayerSessionState`(`feature/PlayerAndSkills`) 로 통합하기로
> 확정됐다. `AShelterPlayerState` 는 비어 있으므로 삭제하고, `AShelterGameMode` 와
> `GS_ShelterGameState` 의 PlayerState 클래스를 교체할 것. 전영배와 함께 처리
> ([01-Architecture.md](../01-Architecture.md) 5장).

> **TODO:** `WBP_ChatBox` 의 노드 재연결 필요. 채팅 RPC 이름이
> `ServerSendChatMessage` → `Server_SendChatMessage` 로 바뀌었다
> ([05-Conventions.md](../05-Conventions.md) 3장).

> **TODO:** `Config/DefaultEngine.ini` 의 `GameDefaultMap` 이 엔진 템플릿을 가리킨다.
> `EditorStartupMap` · `ServerDefaultMap` · `GlobalDefaultGameMode` 도 미설정이라
> **지금 패키징하면 타이틀로 부팅되지 않는다.**

> **TODO:** 정식 레벨 2개가 `Maps/Test/` 아래에 있다. `Maps/Lobby/`, `Maps/Hideout/` 로 옮길 것.

> **TODO:** `Phase.*` 태그(로비 → 은신처 → 준비 → 본작업 → 탈출 → 결과)는 정의만 되어 있고
> 상태 머신이 없다. GameState 뼈대와 함께 착수 필요.

> **TODO:** 플레이어 이름이 `Player_0` 형태의 인덱스 기반이다. `WBP_PlayerNickname` 과
> 연결되어 있지 않다.

> **TODO:** Steam 전환은 결정만 되어 있고 착수하지 않았다
> ([08-Plugins.md](../08-Plugins.md) 3장).
