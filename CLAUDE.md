# HeavyHanded (SNEAKERS)

UE 5.4 / Listen Server 협동 잠입 게임 / 6인 팀.

## 규칙

**작업 전 [Docs/TeamConvention.md](Docs/TeamConvention.md)를 따른다.** 코드·에셋·커밋·네트워크 규칙의 유일한 기준이며,
원본은 [Notion 팀 컨벤션](https://app.notion.com/p/3b2aee16de2380d69049f06b34c9c26c)이다.

특히 자주 어기는 것:

- UObject 포인터 멤버 → 예외 없이 `UPROPERTY()` + `TObjectPtr<T>`
- 게임 상태 변경 → `HasAuthority()` 가드 먼저. 소음 판정은 서버 100%
- 복제 변수 → `GetLifetimeReplicatedProps()` 등록 (빠뜨려도 컴파일 에러가 안 난다)
- 헤더는 전방 선언, `#include`는 `.cpp`에서
- `UPROPERTY` / `UFUNCTION`에 `Category` 필수
- 커밋 메시지 → `feat:` / `fix:` / `docs:` / `style:` / `refactor:` / `test:` / `chore:`

## UE5 함정

**전부 이 프로젝트에서 실제로 밟은 것들이다.** 공통점은 하나 — 컴파일 에러도 경고도 안 난다.

### 1. `UFUNCTION`에 `TObjectPtr`를 쓰면 UHT 에러

`TObjectPtr`는 `UPROPERTY` 멤버용이다. 리플렉션 함수의 파라미터·반환 타입에는 원시 포인터여야 한다.

```cpp
UFUNCTION(BlueprintPure)
const TMap<TObjectPtr<APlayerState>, float>& GetContribution() const;   // X — UHT 에러
```
```
UFunctions cannot take a TObjectPtr as a function parameter or return value.
```

| 위치 | `TObjectPtr` | 원시 포인터 |
|---|---|---|
| `UPROPERTY` 멤버 | ✅ 권장 (컨벤션 2-5) | ⚠️ 되지만 지양 |
| `UFUNCTION` 파라미터 / 반환 | ❌ **UHT 에러** | ✅ 필수 |
| 컨테이너 내부 (`TArray` / `TMap`) | ✅ (맵 키도 가능) | ⚠️ |
| 로컬 변수 | 상관없음 | 상관없음 |

BP에 노출이 필요하면 컨테이너를 그대로 반환하지 말고, 필요한 결과만 뽑는 함수를 따로 만든다.

### 2. `/fp:fast` — 계산된 float을 상수와 직접 비교하지 말 것

UE는 게임 모듈을 **`/fp:fast`** 로 빌드한다 (`VCToolChain.cs` — `FPSemanticsMode.Default`가 imprecise).
컴파일러가 `x / 100.f` 를 `x * 0.01f` 로 바꿔도 되고, `0.01f`는 0.01보다 미세하게 작다.

```
0.67f 리터럴      = 0.670000017  (0x3F2B851F)
67.0f / 100.0f    = 0.669999957  (0x3F2B851E)   ← 1 ULP 아래
```

`67 × 0.01f` 는 아래쪽 float으로 떨어지고 `34 × 0.01f` 는 위로 반올림된다. **값에 따라 되기도 하고 안 되기도 한다** — 재현이 들쭉날쭉해서 원인 찾기가 어렵다.

임계값 비교에는 허용오차를 쓴다. (`Alert/AlertComponent.cpp`의 `AtLeast()` 참고)

```cpp
constexpr float Tolerance = 1.e-4f;
if (Value >= Threshold - Tolerance) { ... }      // O
if (Value >= Threshold)             { ... }      // X — 경계에서 어긋난다
```

`==` 는 `FMath::IsNearlyEqual`을 쓴다.

### 3. 복제는 빠뜨려도 아무 에러가 안 난다

호스트 창에서는 멀쩡히 동작하고 **클라이언트에서만** 값이 안 움직인다. 넷 중 하나라도 빠지면 조용히 실패한다.

1. 소유 액터 생성자 → `bReplicates = true;`
2. 컴포넌트 생성자 → `SetIsReplicatedByDefault(true);`
3. `UPROPERTY(ReplicatedUsing = OnRep_X)` 또는 `Replicated`
4. `GetLifetimeReplicatedProps()` → `DOREPLIFETIME(...)`

**서버에서는 RepNotify가 자동으로 호출되지 않는다.** 리슨 서버의 호스트 창 UI도 갱신되려면 값 변경 후 `OnRep_X()`를 직접 불러야 한다.

검증은 코드 없이 콘솔로 된다. 클라이언트 창에서:

```
obj list class=<컴포넌트클래스>        # 클라에 인스턴스가 생겼는지
DisplayAll <컴포넌트클래스> <프로퍼티>  # 값이 실시간으로 따라오는지 (DisplayClear로 끔)
```

PIE 설정은 Number of Players 2 / **Run Under One Process 해제** / Play As Listen Server.
체크를 해제해야 별도 프로세스로 떠서 진짜 네트워크 경로를 탄다.

### 4. `UActorComponent`에는 `HasAuthority()`가 없다

`AActor`에만 있다. 컴포넌트에서는 소유자의 롤로 판단한다.

```cpp
bool UMyComponent::HasMyAuthority() const
{
	return GetOwnerRole() == ROLE_Authority;
}
```

### 5. 헤더를 고쳤으면 에디터를 닫고 풀 빌드

에디터가 켜진 채로 빌드하면 Live Coding이 막거나(`Unable to build while Live Coding is active`),
빌드가 되더라도 **실행 중인 에디터는 메모리에 올라간 옛 모듈을 계속 쓴다.** 고친 코드로 테스트하고 있다고 착각하기 쉽다.

`Ctrl+Alt+F11`(Live Coding)로는 **UHT 리플렉션 변경을 반영할 수 없다.** `UPROPERTY` / `UFUNCTION` / 클래스 구조를 건드렸으면 에디터를 닫고 빌드한다.

### 6. 런타임 생성 컴포넌트는 디테일 패널에 안 뜬다

`NewObject` + `RegisterComponent`로 붙인 컴포넌트는 에디터 어디에도 노출되지 않는다.
`EditAnywhere`를 달아도 **아무도 못 만지는 하드코딩**이 된다.

기획자가 만질 밸런싱 값은 `UDeveloperSettings`로 뺀다. (`Noise/NoiseSettings.h`, `Alert/AlertSettings.h` 참고)
Project Settings에 페이지가 생기고 ini로 저장되며, PIE 중에 바꿔도 즉시 반영된다.

## 이 저장소에서 알아둘 것

### 게임플레이 태그

태그 본문은 `Config/Tags/<System>.ini`에 시스템별로 분리돼 있다. `Config/DefaultGameplayTags.ini`에는 설정만 둔다.

**`Config/Tags/*.ini`에서는 `+` 접두사를 쓰지 않는다.** 이 파일들은 config 계층이 아니라 개별 파일로 직접 읽히므로
(`FConfigFile::Read` → `ProcessInputFileContents`), `+GameplayTagList=`는 키 이름이 그대로 `"+GameplayTagList"`로 저장돼
`UGameplayTagsList`가 찾지 못한다. **경고 없이 태그 0개**가 된다.

```ini
[/Script/GameplayTags.GameplayTagsList]
GameplayTagList=(Tag="Noise.Loot.Throw",DevComment="...")   ; O
+GameplayTagList=(Tag="...",DevComment="...")               ; X — 조용히 무시됨
```

`+`는 `Config/Default*.ini` 계층 파일에서만 배열 추가 명령으로 처리된다.

### 인코딩 / 개행

- **한글 주석이 있는 `.h` / `.cpp`는 UTF-8 with BOM으로 저장한다.** BOM이 없으면 MSVC가 CP949로 읽어 주석이 개행을 삼키고 다음 줄이 통째로 주석 처리된다.
- `.gitattributes`가 `*.h *.cpp *.ini`를 `text eol=lf`로 강제한다. 파일 끝 개행을 넣을 것.
- `Config/Tags/*.ini`는 BOM 없는 UTF-8 (UE가 BOM 없이도 UTF-8로 디코딩한다).

### 빌드

```powershell
& "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" `
  HeavyHandedEditor Win64 Development `
  -Project="D:\StudyProject\Unreal\HeavyHanded\HeavyHanded.uproject" -WaitMutex -NoHotReload
```

MSVC 14.44 사용 중. UE 5.4 권장은 14.38이라 매 빌드마다 뜨는
`Visual Studio 2022 compiler version 14.44.35227 is not a preferred version` 경고는 **무시해도 된다.**

### 브랜치 / 담당

`main` ← `develop` ← `feature/*`. feature 브랜치는 이미 공유 중이므로 **rebase 금지, merge만 사용**한다.
develop을 받아올 때는 자기 브랜치를 먼저 최신화한 뒤 머지한다.

```bash
git fetch origin
git merge origin/<내 브랜치>    # 먼저 자기 브랜치 최신화
git merge origin/develop
```

| 브랜치 | 담당 | 영역 |
|---|---|---|
| `feature/NoiseAndBoundary` | 김지성 | 소음 · 경계도 |
| `feature/AI` | 유정석 | 경비 AI |
| `feature/PlayerAndSkills` | 전영배 | 플레이어 조작, 스킬 · 패시브 |
| `feature/physics` | 김민준 | 물리, 아이템 |
| `feature/env` | 오유석 | 환경 방해 요소, 협력 장치 |
| `feature/network` | 이지은 | 세션 & 네트워크 |
| `feature/uiux` | — | UI / UX |

**남의 파일을 수정하지 않고 호출만 한다. 남의 클래스를 고치는 대신 컴포넌트를 준다.**

### 시스템 문서

- [Docs/NoiseSystem.md](Docs/NoiseSystem.md) — 소음 · 경계도 시스템. 팀 API, 확정된 설계 결정, 디버그 명령, 열린 항목
