# Data — 데이터 테이블 원본 CSV

여기 있는 CSV 는 `Content/` 의 DataTable 에셋(`DT_*.uasset`)의 **원본**이다.

## 왜 CSV 를 따로 두는가

uasset 은 병합이 안 된다. 두 사람이 같은 표를 고치면 나중에 push 한 쪽이 이기고
다른 쪽 작업은 소리 없이 사라진다.

CSV 는 텍스트라 git 이 줄 단위로 diff 를 보여주고, 서로 다른 행을 고쳤다면
자동으로 병합된다. 그래서 **수치의 진짜 원본은 CSV 쪽이다.**

`Content/` 밖에 두는 이유는 쿠킹 대상에서 빼기 위해서다. 게임에 들어가는 것은
임포트된 uasset 이고, CSV 는 소스일 뿐이다.

## 작업 순서

**값을 고칠 때**

1. 언리얼 에디터에서 DataTable 을 열어 고친다 (중첩 구조체가 펼쳐져서 편집하기 편하다)
2. 표 상단 **Export as CSV** → 여기 있는 파일에 덮어쓴다
3. CSV 와 uasset 을 **같이 커밋한다**

엑셀에서 직접 고쳐도 된다. 그때는 반대로 에디터에서 **Reimport** 한다.
CSV 는 UTF-8(BOM 포함)이라 엑셀에서 한글이 깨지지 않는다.

**노획물을 새로 추가할 때**

먼저 **특성 조합이 기존과 같은지** 본다. 표는 값만 담고, 특성은 BP 의 컴포넌트 조합이
정하기 때문이다.

*조합이 같으면* (메시와 수치만 다르다)

1. 표에 행을 추가한다 (이름 규칙은 아래)
2. 비슷한 `BP_Loot_*` 을 복제하고 메시를 교체한다
3. 디테일의 `Loot|Data > Loot Definition` 에서 새 행을 고른다

*새로운 조합이면* (예: 파손형이면서 중량형)

새 BP 를 만들고 아래대로 컴포넌트를 붙인 뒤 위 3번을 한다.
**태그는 손으로 달지 않는다** — 컴포넌트가 BeginPlay 에서 자기 태그를 등록한다.
그래서 "컴포넌트는 있는데 태그가 없는" 상태가 생기지 않는다.

| 만들 것 | 붙일 컴포넌트 | 자동으로 붙는 태그 |
|---|---|---|
| 파손형 | `LootDurabilityComponent` | `Loot.Type.Fragile` |
| 불안정형 | `LootStabilityComponent` | `Loot.Type.Unstable` |
| 중량형 | 없음 (표에서 `WeightClass=Heavy`) | `Loot.Type.Heavy` |
| 평범 | 없음 | `Loot.Type` |

상속이 아니라 조합이라 특성을 섞어도 클래스가 늘어나지 않는다.
파손형 + 중량형은 `LootDurabilityComponent` 를 붙이고 표에서 `WeightClass=Heavy` 로 두면 된다.

## 행 이름 규칙

`Name` 열이 곧 ID 다. **기본은 `Loot_물건` 하나뿐이다.**

```
Loot_Candlestick        은촛대
Loot_Vase               청화백자
```

특성도 장소도 이름에 넣지 않는다. 둘 다 이미 다른 곳이 알고 있는 정보이고,
이름에 또 적으면 두 벌이 되어 한쪽만 바뀐다.

- **특성**은 표의 `Physics.WeightClass` 와 BP 의 컴포넌트 조합이 정한다.
  `Loot_Heavy_Statue` 로 적어 두면 표에서 무게를 Normal 로 낮추는 순간 이름이 거짓말이 된다.
  조합(중량형 + 경보 연동형인 대형 금고)이 생기면 부를 이름도 없어진다.
- **장소**는 배치의 속성이지 물건의 속성이 아니다. 행 하나는 '한 종류의 물건'이지
  '한 배치'가 아니다. 같은 은촛대를 저택과 박물관에 두면 값이 똑같은 행이 둘 생기고,
  가격을 조정할 때 한쪽만 고치게 된다.

**같은 물건이 장소마다 정말 달라야 할 때만** 뒤에 구분자를 붙인다.

```
Loot_Vase               일반 도자기
Loot_Vase_Museum        박물관 소장품 — 더 비싸고 더 잘 깨진다
```

접두사가 아니라 접미사인 것이 중요하다. 이름순으로 정렬하면 같은 물건의 변형이
나란히 붙어서 무엇이 다른지 바로 비교된다. 앞에 붙이면 표 반대편으로 흩어진다.

**이름은 처음에 정하고 바꾸지 않는다.** `FDataTableRowHandle` 은 이름으로 참조해서,
행 이름을 바꾸면 그 행을 쓰던 BP 가 전부 연결이 끊긴다. 끊겨도 로그 경고만 뜨고
조용히 BP 인라인 값으로 돌기 때문에 발견이 늦어진다.

## 파일

| CSV | DataTable | 행 구조체 | 담당 |
|---|---|---|---|
| `LootCatalog.csv` | `Content/HeavyHanded/Data/DataTables/DT_LootCatalog` | `FLootDefinitionRow` | 김민준 |

## 주의

`Physics` 와 `Stability` 열은 구조체가 통째로 들어가 있어서 괄호 안에 필드가 나열된다.
엑셀에서 이 칸을 직접 고치는 것은 권하지 않는다 — 괄호나 쉼표 하나가 어긋나면
그 행 전체가 기본값으로 임포트된다. **물리 수치는 에디터에서 고치는 것이 안전하다.**

`Stability` 는 `ULootStabilityComponent` 를 붙인 노획물(불안정형)만 쓴다.
컴포넌트가 없는 행에도 칸은 있지만 채워도 아무 일이 일어나지 않는다 —
특성은 표가 아니라 컴포넌트 조합이 정하기 때문이다.

그 착오를 잡으려고 `ALootBase` 가 스폰 시 경고를 찍는다. PIE 로그에 이게 뜨면
표가 아니라 BP 를 봐야 한다는 뜻이다.

```
[Loot:...] 표에 불안정형 수치(Stability)가 적혀 있는데 ULootStabilityComponent 가 없다
[Loot:...] 표의 MaxImpactCount 가 2 인데 ULootDurabilityComponent 가 없다
```

반대로 `Loot|Tags > Loot Type Tags` 에 `Loot.Type` 하나만 있으면 컴포넌트를 빼먹은 것이다.
파손형이면 `Loot.Type.Fragile`, 불안정형이면 `Loot.Type.Unstable` 이 자동으로 붙어 있어야 한다.

`DamageImpulseThreshold` 는 질량에 비례한다. `MassKg` 를 바꾸면 같이 조정해야 한다.
10kg 기준 실측값: 100cm 낙하 5031 / 150cm 6497 / 300cm 9622, 착지 후 튕김은 500~900.
