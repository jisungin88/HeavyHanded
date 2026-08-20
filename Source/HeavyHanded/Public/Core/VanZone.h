#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VanZone.generated.h"

struct FHeistLoadEntry;
class ABaseCharacter;
class ACharacter;
class ALootBase;
class APawn;
class APlayerState;
class UBoxComponent;
class UDecalComponent;
class UMaterialInterface;
class UNiagaraSystem;

/**
 * 밴. 코어 루프에서 "돈이 들어오는" 지점이자 "판이 끝나는" 지점이다.
 *
 * [볼륨이 둘이고 서로를 모른다]
 *   LoadVolume   노획물만 Overlap (프로파일 VanLoadZone)  → 적재 판정
 *   BoardVolume  플레이어만 Overlap (프로파일 VanBoardZone) → 승차 가능 범위
 *
 *   콜백도 판정도 따로다. 크기를 따로 잡을 수 있게 나눠 둔 것이다 — 화물칸은 좁게,
 *   승차 범위는 뒷문 발판까지 넓게 잡는 배치가 자연스럽다.
 *   한 액터에 둔 이유는 밴이 하나이기 때문이다. 둘로 나누면 레벨 담당이 배치물 두 개의
 *   상대 위치를 매번 맞춰야 하고, 밴이 움직이게 되면 어태치도 두 번 해야 한다.
 *
 * [적재와 승차는 성격이 다르다]
 *   노획물은 갖다 놓으면 알아서 실린다. 사람은 상호작용해야 탄다.
 *
 *   그래서 BoardVolume 은 오버랩 콜백을 쓰지 않는다 — 볼륨을 드나드는 것 자체는
 *   아무 일도 일으키지 않고, 상호작용이 들어온 순간에 "지금 이 안에 있는가" 만 물어본다.
 *   승차를 명시적 행동으로 만든 이유는 실수로 판이 끝나는 것을 막기 위해서다.
 *   전원 승차는 곧 판의 종료라, 걸어 들어간 것만으로 성립하면 안 된다.
 *
 *   타는 것과 내리는 것은 대칭이 아니다. 탈 때는 밴을 겨눠야 하지만 내릴 때는 그럴 필요가 없다 —
 *   탑승하면 몸이 고정되어 조준을 요구하는 것이 부당해진다. TryDisembarkIfBoarded 참고.
 *
 *   조준 대상은 볼륨이 아니라 BoardAimTarget 이다. 큰 볼륨을 시선에 걸리게 만들면 화물칸 안에 있는
 *   노획물보다 볼륨이 먼저 맞아서, 밴 안의 물건을 영영 집을 수 없게 된다.
 *
 *   BoardVolume 은 BoardAimTarget 앞에 서면 안에 들어오도록 잡는다. 문을 겨눌 수 있는 자리가
 *   곧 탈 수 있는 자리여야 헷갈리지 않는다.
 *
 * [밴 몸체는 BP 가 갖는다 — 별도 액터로 두지 않는다]
 *   C++ 은 판정에 필요한 것만 갖는다. 보이는 것(몸체 · 문짝 3개 · 바퀴)은 `BP_VanLoadZone`
 *   에 StaticMeshComponent 로 붙인다.
 *
 *   **몸체를 별도 액터로 두면 안 된다.** AHeistGameMode::PlaceVan 이 이 액터만 진입점으로
 *   옮기기 때문에, 몸체가 다른 액터면 밴이 갈 때 껍데기만 제자리에 남는다.
 *   레벨 배치도 두 개가 되어 상대 위치를 매번 맞춰야 한다 (볼륨을 한 액터에 둔 것과 같은 이유).
 *
 *   **몸체 콜리전이 화물칸을 막지 않아야 한다.** 자동 생성된 박스 콜리전을 그대로 쓰면
 *   던져 넣은 노획물이 튕겨 나온다. 개구부가 뚫린 커스텀 콜리전을 쓸 것.
 *
 *   움직이는 문짝(후면 좌·우, 측면 슬라이드)은 순수 비주얼이다. 조준은 BoardAimTarget 이 받는다.
 *
 * [레벨당 하나] 진입 지역에 따라 놓이는 자리는 달라지지만, 한 레벨에 이 액터는 하나뿐이다.
 *   BeginPlay 에서 개수를 세어 둘 이상이면 경고를 남긴다 — 계약이 말로만 남으면
 *   누가 복사해 붙여도 아무도 모르고, 적재액이 어느 존에서 왔는지 추적할 수 없게 된다.
 *
 * [무엇을 판정하고 무엇을 판정하지 않는가]
 *   여기가 보는 것    존 안에 있는가 / 지금 누가 들고 있는가 / 얼마나 오래 있었는가
 *   보지 않는 것      가치가 얼마인가 · 파손됐는가 · 무거운가 — 전부 노획물 파트(김민준) 소관이다
 *                     ALootBase 에 물어본 값을 그대로 믿는다.
 *
 *   (기술설계서 코어 루프 5장 "경계 — 이게 적재 가능한 물건인가는 아이템 파트가 판정한다")
 *
 * [확정 조건 — 운반자 없이 존 안에서 UHeistSettings::LoadDwellSeconds 만큼 머물기]
 *   노획물의 물리는 확정 전까지 그대로 살아 있다. 그래서 오버랩만으로는 판정이 안 된다 —
 *   던져 넣은 물건이 화물칸 바닥에서 튕겨 다시 굴러 나올 수 있고, 그것까지 적재로 세면
 *   밴 쪽으로 던지기만 해도 돈이 들어온다. 체류 시간이 "제대로 들어갔는가" 를 가른다.
 *
 *   들고 있는 동안은 카운트가 멈춘다. 손에서 떠난 순간부터가 시작이다 — 들고 서 있는 것만으로
 *   적재되면 밴 옆을 지나가기만 해도 돈이 들어오고, "실을까 말까" 라는 판단 자체가 사라진다.
 *
 *   놓기와 던지기는 오버랩 이벤트를 새로 만들지 않는다. 물건은 이미 존 안에 있고 바뀌는 것은
 *   운반자뿐이라, 주기적으로 다시 물어보지 않으면 영원히 확정되지 않는다. 재검사 타이머가 그것이다.
 *
 * [확정되면 노획물은 사라진다]
 *   물리를 끄고 화물칸에 붙여 두지 않는다. 실린 물건이 남아 있으면 밴이 움직일 때 흘러내리고,
 *   클라이언트에서 물리가 따로 돌아 바닥을 뚫고, 그걸 맞추려고 어태치 상태를 복제해야 한다.
 *   사라지면 그 전부가 필요 없어진다 — 액터 파괴는 엔진이 알아서 복제한다.
 *
 *   무엇을 실었는지는 확정 순간에 FHeistLoadEntry 로 복사해 AHeistGameState 에 넘긴다.
 *   결과 화면은 액터가 아니라 그 기록을 본다.
 *
 * [연출을 갈아 끼우는 자리] HandleConfirmedLoot() 하나다. 기본 구현은 이펙트 후 파괴이고,
 *   "NPC 가 나와서 밴에 싣는다" 로 바꾸려면 그 함수만 재정의하면 된다.
 *   판정 · 금액 · 이벤트는 그 위에서 이미 끝나 있어 건드릴 것이 없다.
 *
 * [승차는 상태이지 사건이 아니다] 노획물과 달리 플레이어는 탔다가 내릴 수 있다 —
 *   동료를 구하러 나가는 것은 정상적인 플레이다. 그래서 체류 시간도 확정도 없고,
 *   상호작용 한 번이 곧 명단의 추가이거나 제거다.
 *
 *   "언제 판이 끝나는가" 는 여기서 정하지 않는다. 이 액터는 명단만 갱신하고,
 *   전원이 탔는지 보는 것은 AHeistGameMode 다 — 판정 계기가 승차 말고도 둘 더 있어서
 *   (다운 · 접속 종료) 여기 두면 규칙이 세 곳으로 흩어진다.
 *
 * 판정은 전부 서버다. 클라이언트가 하는 일은 확정 이펙트를 재생하는 것뿐이다.
 */
UCLASS()
class HEAVYHANDED_API AVanZone : public AActor
{
	GENERATED_BODY()

public:
	AVanZone();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** 이 레벨의 밴. 레벨당 하나라는 계약 위에서만 성립한다 (BeginPlay 가 중복을 경고한다) */
	UFUNCTION(BlueprintPure, Category = "Van", meta = (WorldContext = "WorldContext"))
	static AVanZone* Get(const UObject* WorldContext);

	/**
	 * 탑승 중인 사람이면 내려 준다. 아니면 아무 일도 하지 않는다. (서버 전용)
	 *
	 * [왜 따로 있는가] 탑승하면 몸이 밴에 고정된다. 그 상태에서 하차하려고 밴 메시를
	 *   다시 겨누게 하면, 차 안에서 벽을 조준해야 내릴 수 있는 꼴이 된다.
	 *   시선이 어디를 향하든 내릴 수 있어야 한다.
	 *
	 *   그래서 UGAB_Interact 는 시선 스윕보다 **먼저** 이것을 부른다. 탑승 중이라면
	 *   조준 결과를 아예 보지 않는다 — 밴 안에서 할 수 있는 일은 내리는 것뿐이다.
	 *
	 * @return 실제로 내렸으면 true. 타고 있지 않았으면 false (호출부는 평소대로 진행하면 된다)
	 */
	static bool TryDisembarkIfBoarded(APawn* Player);

	/**
	 * 승차 / 하차를 전환한다. 상호작용이 들어오는 유일한 입구다. (서버 전용)
	 *
	 * 부르는 쪽은 UGAB_Interact 이고, 그쪽은 "밴을 조준한 채 눌렀다" 만 안다.
	 * 탈 수 있는 상황인지(뒷칸 안에 있는가 · 지금 그럴 페이즈인가)는 전부 여기서 본다 —
	 * 조건을 어빌리티에 두면 플레이어 파트가 코어 루프의 규칙을 알아야 한다.
	 *
	 * @return 상태가 실제로 바뀌었으면 true. 거부됐으면 false
	 */
	bool TryToggleBoarding(APawn* Player);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 액터의 기준점. **밴 바닥 중앙이고, 로컬 +X 가 뒷문 방향이다.**
	 *
	 * [왜 판정 볼륨이 아니라 빈 컴포넌트가 루트인가]
	 *   볼륨을 루트로 두면 액터 원점이 화물칸 한가운데 공중에 뜬다. 그러면 바닥에 놓이는
	 *   것들(좌석 · 하차 지점)의 Z 가 전부 화물칸 크기에 묶여서, 볼륨을 늘리는 순간
	 *   좌석이 공중에 뜨고 문짝이 볼륨 안에 파묻힌다.
	 *
	 *   바닥 기준점을 따로 두면 그것들이 전부 Z=0 이라 볼륨 크기와 무관해진다.
	 *   레벨에 배치할 때도 이 편이 맞다 — 밴을 놓는 사람은 바닥에 스냅하지,
	 *   화물칸 중심 높이를 계산하지 않는다.
	 *
	 *   **배치를 손보는 일이 줄어드는 것이지 코드가 대신 놓아 주는 것은 아니다.**
	 *   아래 컴포넌트들의 위치 · 크기 · 회전은 전부 뷰포트에서 정하고, 코드는 덮어쓰지 않는다.
	 *
	 * [진입점과의 관계] AHeistEntryPoint 의 VanAnchor 화살표가 이 지점으로 온다
	 *   (AHeistGameMode::PlaceVan). 화살표를 바닥에, 화살표 방향을 뒷문 쪽으로 놓으면 된다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<USceneComponent> VanRoot;

	/**
	 * 적재 판정 볼륨. 프로파일은 VanLoadZone (Config/DefaultEngine.ini)
	 *
	 * 기본값은 바닥이 VanRoot 평면에 닿는 높이다. **크기와 위치는 뷰포트에서 정한다** —
	 * 크기를 바꿨으면 문짝과 하차 지점도 같이 봐 줘야 한다. 코드는 덮어쓰지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> LoadVolume;

	/**
	 * 승차 가능 범위. 프로파일은 VanBoardZone — 플레이어만 Overlap 한다.
	 *
	 * 오버랩 콜백을 걸지 않는다. 이 볼륨이 하는 일은 상호작용이 들어온 순간
	 * "이 사람이 지금 뒷칸 안에 있는가" 한 번 답해 주는 것뿐이다.
	 *
	 * 적재 볼륨과 크기가 같을 이유가 없어서 따로 둔다. 화물칸 깊숙이 던져 넣어야 실리지만,
	 * 사람은 뒷문에 발만 걸쳐도 탈 수 있게 해 주는 편이 도주 중에 덜 억울하다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> BoardVolume;

	/**
	 * 승차 상호작용의 조준 대상. 뒷문 **개구부**를 덮는 안 보이는 판이다.
	 *
	 * [문짝이 아니라 개구부인 이유] 보이는 문짝은 BP 가 실제 메시로 따로 갖고, 그것들은
	 *   열리고 닫히며 움직인다. 조준 대상을 그 메시에 걸면 **문이 열리는 순간 조준 대상이
	 *   옆으로 돌아가 버려서, 정작 탈 수 있게 된 순간에 탈 수 없게 된다.**
	 *   이 판은 개구부 자리에 고정돼 문이 어떤 상태든 조준을 받는다.
	 *
	 *   후면 문이 둘(좌·우)이라는 것도 이유다. 문짝 하나를 조준 대상으로 삼으면
	 *   반대쪽을 겨눈 사람은 타지 못한다.
	 *
	 * [왜 메시가 아니라 박스인가] 조준만 받으면 되는데 메시일 이유가 없다.
	 *   예전에는 엔진 큐브를 얇게 눌러 썼는데, 그러면 콜리전이 메시의 BodySetup 에서 나와서
	 *   **메시를 비우는 순간 조준이 통째로 죽는다.** 박스는 자기가 형상을 갖는다.
	 *
	 * [콜리전이 Visibility 뿐인 이유] UGAB_Interact 의 시선 스윕이 그 채널을 쓰므로
	 *   이걸 열면 승차가 아예 불가능해진다. 반대로 Pawn 을 막으면 열린 뒷문이 벽이 되어
	 *   걸어 나올 수 없다.
	 *
	 * 크기와 위치는 실제 밴 메시의 개구부에 맞춰 뷰포트에서 정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van")
	TObjectPtr<UBoxComponent> BoardAimTarget;

	/**
	 * 좌석 앵커. 탑승한 플레이어가 여기로 옮겨져 붙는다.
	 *
	 * [왜 앵커를 여러 개 두는가] 좌석 배치는 밴 실내 모양에 달렸고 코드는 그 모양을 모른다.
	 *   앵커 하나를 놓고 코드가 좌우로 벌리면 실내가 좁을 때 벽을 뚫는다.
	 *   배치자가 뷰포트에서 직접 놓게 두는 편이 언제나 맞다.
	 *
	 * [배정] 승차하면 빈 자리 중 첫 번째를 차지하고, 하차하면 그 자리가 빈다.
	 *   좌석이 모자라면 마지막 좌석에 겹쳐 앉힌다 — 승차를 거부하면 그 사람은 영영 탈출하지
	 *   못하는데, 겹쳐 보이는 것이 그보다 훨씬 낫다.
	 *
	 * [복제] 배정은 서버만 안다. 어태치 대상과 상대 위치가 복제되므로 클라이언트는
	 *   좌석이라는 개념을 몰라도 같은 그림을 만든다.
	 *
	 * 앵커 위치는 '발이 닿는 지점' 이다. 캡슐 절반 높이는 코드가 더한다 —
	 * 배치자가 캐릭터 캡슐 크기를 알아야 자리를 놓을 수 있으면 안 된다.
	 *
	 * VanRoot 가 바닥 기준이므로 좌석의 기본 Z 는 0 이다. 볼륨 크기를 바꿔도 움직이지 않는다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van|Seats")
	TArray<TObjectPtr<USceneComponent>> Seats;

	/**
	 * 하차 지점. 내린 사람이 여기에 선다.
	 *
	 * [좌석에 그대로 내려놓지 않는 이유] 좌석과 하차 지점은 요구 조건이 다르다.
	 *   좌석은 어디든 된다 — 어태치된 캐릭터는 이동 판정을 하지 않으므로 공중이든 메시
	 *   안이든 상관없다. 하차 지점은 걸어 다닐 수 있는 바닥이어야 한다.
	 *   좌석 자리에 그냥 내려놓으면 앉을 자리로 고른 지점에 걷는 캐릭터를 세우는 셈이라,
	 *   두 조건이 우연히 겹칠 때만 동작한다.
	 *
	 *   밴 뒷문 바깥 바닥에 놓는 것이 기본이다. 실내가 넓은 밴이라면 안쪽에 놓아도 된다 —
	 *   중요한 것은 '설 수 있는 곳' 이라는 조건뿐이다.
	 *
	 * 좌석과 마찬가지로 발이 닿는 지점으로 해석한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van|Seats")
	TObjectPtr<USceneComponent> ExitAnchor;

	/**
	 * 존의 범위를 바닥에 그리는 데칼. 크기는 볼륨에서 자동으로 따라간다.
	 *
	 * 판정 볼륨은 게임 화면에 보이지 않으므로, 표시가 없으면 플레이어가 "어디까지가 밴인지"
	 * 를 알 방법이 없다. 반투명 박스 대신 바닥 데칼인 이유는 화물칸 안쪽 시야를 가리지 않기
	 * 때문이다 — 물건을 던져 넣는 동안 안이 보여야 한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UDecalComponent> BorderDecal;

	/**
	 * 테두리 데칼에 쓸 머티리얼. 비워 두면 데칼이 꺼지고 디버그 박스로 대신 그린다.
	 *
	 * 폴백을 둔 이유는 그레이박스 때문이다. 머티리얼이 나오기 전에도 레벨 담당이 밴을 배치하고
	 * 존 범위를 눈으로 확인할 수 있어야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UMaterialInterface> BorderMaterial;

	/**
	 * 적재 확정 이펙트. 모든 머신에서 확정 지점에 재생된다.
	 *
	 * 비워 두면 노획물이 아무 연출 없이 사라진다 — 동작에는 지장이 없지만
	 * "실린 것" 과 "어디론가 사라진 것" 이 화면상 구별되지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Visual")
	TObjectPtr<UNiagaraSystem> LoadedEffect;

	/**
	 * 보류 중인 노획물을 다시 검사하는 주기(초).
	 *
	 * 체류 시간(UHeistSettings::LoadDwellSeconds)을 재는 해상도다. 확정이 최대 이만큼 늦어질 수
	 * 있으므로 체류 시간보다 충분히 작아야 한다. 검사 대상은 존 안에 있는 것뿐이고 하는 일이
	 * 포인터 확인과 뺄셈이라 비용이 없다. 검사할 것이 없으면 타이머 자체가 멈춘다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van",
		meta = (ClampMin = "0.02", Units = "s"))
	float RecheckIntervalSeconds = 0.1f;

	/**
	 * 적재 판정 과정을 로그와 화면에 남긴다. (테스트용, 기본 꺼짐)
	 *
	 * 확정뿐 아니라 대기·거부도 사유와 함께 찍는다. "밴에 넣었는데 돈이 안 오른다" 는
	 * 신고가 오면 존 밖이었는지 · 아직 들고 있었는지 · 체류 시간을 못 채우고 굴러 나갔는지를
	 * 갈라야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Van|Debug")
	bool bShowLoadDebug = false;

	/**
	 * 확정된 노획물을 처리한다. 이 함수를 지나면 적재는 이미 끝나 있다. (서버 전용)
	 *
	 * 기본 구현은 확정 이펙트를 전원에게 재생하고 노획물을 파괴한다.
	 * "NPC 가 나와서 밴에 싣는다" 로 바꾸려면 여기를 재정의해 파괴를 미루고 NPC 에 넘기면 된다 —
	 * 금액 · 기록 · 이벤트는 이미 반영된 뒤라 이 함수가 무엇을 하든 판정은 흔들리지 않는다.
	 *
	 * [재정의할 때의 계약] 노획물을 남겨 두기로 했다면 콜리전을 직접 정리할 것.
	 *   살려 두면 존 안에 그대로 있어 다음 오버랩에 다시 걸린다. 기본 구현은 파괴하므로
	 *   그 문제가 없다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Van")
	void HandleConfirmedLoot(ALootBase* Loot);
	virtual void HandleConfirmedLoot_Implementation(ALootBase* Loot);

	/**
	 * 확정 이펙트를 전원에게 재생한다.
	 *
	 * 노획물 액터가 아니라 이 존이 보낸다. 노획물은 곧 파괴되는데, 파괴되는 액터의 채널로
	 * 보낸 RPC 는 도착 여부를 장담할 수 없다. 존은 레벨 내내 살아 있다.
	 *
	 * Unreliable 인 이유는 연출이기 때문이다. 한 번 놓쳐도 금액과 기록은 이미 복제된다.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayLoadedEffect(FVector Location);

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	/**
	 * 지금 이 사람이 탈 수 있는가. 거부 사유를 로그로 남긴다.
	 *
	 * [준비 시간에는 못 탄다] 본 작업 전에 타 봐야 할 일이 없고, 그때 이동이 묶이면
	 *   준비 시간을 통째로 날린다. 도주 시간에는 당연히 탈 수 있다.
	 */
	bool CanBoardNow(const APawn* Player) const;

	/**
	 * 탑승한 폰을 좌석에 앉히거나 내려놓는다. (서버 전용, 결과는 복제된다)
	 *
	 * 명단만 갱신하고 폰을 그대로 두면 "탄 사람" 이 저택까지 걸어갈 수 있다.
	 * 승차가 위치와 무관한 표시가 되어 버리므로, 자리로 옮기고 이동을 묶는다.
	 *
	 * [이동 차단을 여기서 하는 이유] 정석은 State.InVan 에 Block.Movement 를 물리는
	 *   GameplayEffect 이고 그건 전영배 영역이다. 그 GE 가 생기면 이 함수의 이동 차단
	 *   부분은 걷어내고 자리 배정과 어태치만 남긴다.
	 */
	void ApplyBoardedPawnState(APawn* Player, bool bBoarded);

	/**
	 * 빈 좌석을 하나 배정한다. 이미 앉아 있으면 그 자리를 그대로 돌려준다.
	 *
	 * 비어 있는지는 점유자가 살아 있고 아직 승차 명단에 있는지로 본다.
	 * 접속이 끊긴 사람의 자리가 영영 잠기지 않도록, 판정을 상태가 아니라 사실에 건다.
	 *
	 * @return 배정된 좌석. 좌석이 하나도 없으면 nullptr
	 */
	USceneComponent* TakeSeat(APlayerState* Player);

	/** 좌석을 비운다. 앉아 있지 않았으면 아무 일도 하지 않는다 */
	void ReleaseSeat(const APlayerState* Player);

	/**
	 * 캐릭터를 앵커 위치에 세운다. 앵커는 발이 닿는 지점으로 해석한다.
	 *
	 * 캡슐 원점은 가운데라, 앵커 좌표를 그대로 넣으면 절반이 바닥에 파묻힌다.
	 * 그 보정을 호출부마다 다시 쓰면 한 곳만 빠져도 그 자리에서만 캐릭터가 가라앉는다.
	 */
	static void PlaceAtAnchor(ACharacter* Character, const USceneComponent* Anchor);

	/** 승차자의 ASC 로 Event.Player.BoardedVan 을 보낸다. 내릴 때는 보내지 않는다 */
	void SendBoardedEvent(APawn* Player, int32 NumBoarded) const;

	/**
	 * 지금 적재를 받는 페이즈인가. 노획물의 상태와는 무관한, 판 전체의 사정이다.
	 *
	 * 운반자 여부와 체류 시간을 여기서 보지 않는 이유는 그것이 '거부' 가 아니라 '대기' 이기
	 * 때문이다. 셋을 한 함수에 넣으면 false 하나로 "영영 안 됨" 과 "이따 다시" 가 구별되지 않는다.
	 */
	bool IsLoadAllowedNow() const;

	/** 존에 들어온 노획물을 추적 목록에 넣고 재검사 타이머를 켠다. (서버 전용) */
	void TrackPending(ALootBase* Loot);

	/** 추적 중인 것들의 체류 시간을 갱신하고, 다 채운 것을 확정한다. (서버 전용) */
	void RecheckPending();

	/** 추적 목록이 비어 있지 않을 때만 타이머가 돈다 */
	void UpdateRecheckTimer();

	/** 적재를 확정한다. 여기서만 금액이 오른다. (서버 전용) */
	void ConfirmLoad(ALootBase* Loot);

	/** 확정 순간의 사실을 값으로 복사한다. 액터가 사라져도 남아야 하는 것들이다 */
	FHeistLoadEntry MakeLoadEntry(const ALootBase* Loot) const;

	/** 적재자의 ASC 로 Event.Loot.Loaded 를 보낸다. 적재자를 모르면 아무것도 하지 않는다 */
	void SendLoadedEvent(APawn* Loader, ALootBase* Loot, int32 LoadedValue) const;

	/** 데칼 크기를 볼륨에 맞춘다. 에디터에서 볼륨을 늘리면 테두리도 같이 늘어난다 */
	void SyncBorderDecal();

	/** 머티리얼이 없을 때 존 범위를 디버그 선으로 대신 그린다 */
	void DrawBorderFallback() const;

	/** 레벨에 이 액터가 둘 이상이면 경고한다 */
	void WarnIfDuplicateZone() const;

	void ShowLoadDebug(const FString& Message, const FColor& Color, const FVector& Location) const;

	/**
	 * 존 안에 있고 아직 확정되지 않은 노획물과, 그 물건이 '손을 떠난 시각'(월드 시간).
	 *
	 * 값이 음수면 아직 누가 들고 있다는 뜻이다 — 그 상태에서는 체류 시간이 흐르지 않는다.
	 * 들었다 놓기를 반복하면 그때마다 처음부터 다시 센다.
	 *
	 * 키가 약참조인 이유는 여기 있는 동안 노획물이 파괴될 수 있기 때문이다 (파손형).
	 * 그때는 다음 검사에서 조용히 빠진다.
	 */
	TMap<TWeakObjectPtr<ALootBase>, float> PendingLoot;

	/**
	 * 좌석별 점유자. 인덱스는 Seats 와 같다.
	 *
	 * 복제하지 않는다 — 클라이언트가 알아야 하는 것은 "누가 어디에 붙어 있는가" 이고
	 * 그건 어태치 복제가 이미 전달한다. 좌석 번호 자체는 서버의 장부일 뿐이다.
	 */
	TArray<TWeakObjectPtr<APlayerState>> SeatOccupants;

	FTimerHandle RecheckTimerHandle;
};
