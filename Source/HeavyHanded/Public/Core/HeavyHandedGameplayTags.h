#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * HeavyHanded 게임플레이 태그 네이티브 선언.
 *
 * 태그를 문자열로 조회하면 오타를 컴파일러가 잡지 못하고 런타임에 조용히 실패한다.
 * C++ 에서 참조하는 태그는 여기에 선언하고, 문자열 정의는 Config/Tags/<시스템>.ini 에
 * 그대로 둔다 — 둘은 짝이다.
 *
 * [범위] 지금 C++ 이 실제로 참조하는 태그만 선언한다.
 *   .ini 의 태그를 미리 다 옮겨오지 않는다 — 쓰지 않는 선언은 또 하나의 목록이 되고,
 *   .ini 와 어긋나도 아무도 모른다.
 *
 * [소유] 각 태그는 해당 .ini 파일 담당자 소유다 (문서 06 협업 규칙).
 *   남의 시스템 태그를 여기 추가할 때는 담당자에게 먼저 알린다.
 *
 * [경계] 물리·아이템 파트는 태그 대신 FLootImpactEvent(물리적 사실)만 방송한다.
 *   그것이 얼마나 시끄러운지는 소음 파트가 해석한다.
 */
namespace HHTags
{
	// ── 소음 (지성인 / Config/Tags/Noise.ini) ──

	/** 물건 충돌. UNoiseEmitterComponent 가 에디터 미지정 시 쓰는 기본값 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Noise_Loot_Impact);

	/** 파손형 노획물이 깨질 때 나가는 태그 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Noise_Loot_Break);

	// ── 코어 루프 페이즈 (지성인 / Config/Tags/Phase.ini) ──
	//
	// AHeistGameState 가 하나를 들고 전원에게 복제한다. 판정은 전부 서버(AHeistGameMode)다.

	/** 준비 45초 — 역할 선택 · 드론 사전 정찰. 미션 제한 시간에 포함되지 않는다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase_Prep);

	/** 본 작업 — 제한 시간은 장소마다 다르다 (AHeistGameMode::HeistSeconds) */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase_Heist);

	/** 도주 90초 — 경보 100% 또는 제한 시간 만료로 진입한다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase_Escape);

	/** 결과 — 적재 목록 · 기여도 · 최다 소음 유발자 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase_Result);

	// Phase.Lobby / Phase.Hideout 은 선언하지 않는다.
	//   그 둘은 레벨 자체가 다르고 전환 수단이 ServerTravel 이라 태그로 판정할 것이 없다.
	//   작업 레벨의 상태머신은 Prep 부터 시작한다.
	//   Site.* 도 아직이다 — 목표 금액·제한 시간을 GameMode 프로퍼티로 받고 있어
	//   지금은 C++ 이 장소를 태그로 구분할 일이 없다.

	// ── 노획물 (김민준 / Config/Tags/Loot.ini) ──

	/**
	 * 특성 태그의 부모. "집을 수 있는 노획물인가" 판정이 이 하나로 끝난다.
	 *
	 * FGameplayTagContainer::HasTag 는 부모 매칭이라 Loot.Type.Heavy 같은 하위 태그가
	 * 전부 걸린다. 플레이어 파트의 GAB_Interact 가 이 방식으로 집기 대상을 고른다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type);

	/** 중량형 — 2인이어야 제 속도가 난다. 던지기 불가 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type_Heavy);

	/** 파손형 — ULootDurabilityComponent 가 붙어 있으면 자동으로 달린다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type_Fragile);

	/** 불안정형 — ULootStabilityComponent 가 붙어 있으면 자동으로 달린다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_Type_Unstable);

	// 상태 태그. 특성과 달리 런타임에 붙었다 떨어진다.

	/** 배치 상태 — 아무도 안 들었고 아직 옮겨진 적 없다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Idle);

	/** 1인 운반 중 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Carried);

	/** 바닥에 놓임. 환경 파트의 압력판이 이 태그로 판정한다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Dropped);

	/** 파괴됨 — 가치 0 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Broken);

	/**
	 * 밴 화물칸 적재 완료 — 정산 대상.
	 *
	 * [소유 주의] Loot.* 는 김민준 소유인데 선언을 여기 올린 것은 코어 루프(AVanLoadZone)다.
	 *   Config/Tags/Loot.ini 에 이미 정의돼 있던 태그라 이름을 새로 만든 것은 아니고,
	 *   상태를 실제로 붙이는 쪽이 적재존이라 C++ 참조가 코어 루프에서 먼저 생겼다.
	 *   노획물 파트가 이 상태를 직접 다루게 되면 소유는 그쪽으로 넘어간다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Loot_State_Loaded);

	// Loot.State.Spilled 는 아직 선언하지 않는다.
	//   상태 태그는 배타적인데(하나만 유효), 유출은 들고 가는 도중에도 일어나므로
	//   Carried 를 밀어내 버린다. 파괴는 액터가 곧 사라져서 문제가 없지만 유출은 다르다.
	//   "샌 적이 있다" 는 사실이 필요해지면 상태가 아닌 별도 컨테이너로 붙일 것.
	//   지금은 ULootStabilityComponent::GetSpillCount() 와 ALootBase::IsValueLost() 로 읽는다.

	// ── 플레이어 상태 (전영배 / Config/Tags/State.ini) ──

	/** 다운 — 무장 경비 접촉 / 경비견 3회 / 낙하. 동료가 5초간 복구한다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Downed);

	/**
	 * 뛰기(Shift 홀드) 중. SprintGameplayEffectClass 가 부여/해제한다.
	 * ABaseCharacter::OnSprintTagChanged 가 이 태그의 추가/제거를 구독해 Noise.Player.Run
	 * 반복 발행 타이머를 켜고 끈다 (Config/Tags/State.ini 의 원래 설계 의도 그대로).
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);

	/**
	 * 밴 승차 완료.
	 *
	 * [진리원이 아니다] 탈출 판정의 근거는 AHeistGameState 의 승차 명단이고, 이 태그는
	 *   그 결과를 남에게 보이기 위한 미러다. 태그로는 인원을 셀 수 없고(전원 순회해야 한다)
	 *   순서도 시각도 담기지 않아서, 판정을 여기에 걸면 세는 시점마다 답이 달라진다.
	 *   태그가 있는 이유는 경비 AI · 어빌리티 차단 · HUD 가 코어 루프 헤더를 include 하지 않고도
	 *   "이 사람 밴 안에 있나" 를 물어볼 수 있게 하기 위해서다.
	 *
	 * [부여 주체 — 지금은 코어 루프다] AHeistGameState 가 명단을 갱신하면서 복제 Loose 태그로
	 *   붙인다. State.* 는 전영배 소유이므로, 그쪽 GameplayEffect 가 같은 태그를 다루게 되면
	 *   부여 주체를 그쪽으로 넘기고 여기서는 걷어낼 것. 두 주체가 동시에 붙이면 태그는
	 *   카운트 방식이라 한쪽이 빼도 다른 쪽 카운트가 남아 영영 안 없어진다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_InVan);

	/** 체포 — 도주 시간이 끝날 때까지 밴에 못 탔거나, 그 시점에 다운 상태였다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Arrested);

	// ── 어빌리티 (전영배 / Config/Tags/Ability.ini) ──

	/**
	 * 협력 캐리 보조 — 중량형의 반대편을 잡는다. 기획서 4장 조작 표의 '협력 캐리(E)'.
	 *
	 * 한 태그가 두 가지로 쓰인다 — UGA_HeavyCarryAssist 의 GameplayEvent 트리거이자
	 * 그 어빌리티의 식별 태그다. GAB_Interact 가 이 태그로 이벤트를 보내고,
	 * 보조자가 자발적으로 놓을 때는 같은 태그로 CancelAbilities 를 건다.
	 *
	 * [왜 네이티브 선언이 필요했나] 예전에는 생성자에서 RequestGameplayTag 로 문자열
	 *   조회를 했는데, .ini 에 태그가 없어서 CDO 생성 중에 check() 가 터졌다.
	 *   그 시점이 모듈 로드라 **에디터가 아예 뜨지 않았다.** 네이티브 태그는 정적 초기화
	 *   때 등록되므로 .ini 가 비어 있어도 살아남고, 오타는 컴파일러가 잡는다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_HeavyCarryAssist);

	/** 공통 소모품 슬롯. GAB_Throw 가 클라이언트에서 서버로 올리는 이벤트 태그로 쓴다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Slot_Consumable);

	// ── 게임플레이 이벤트 (공용 / Config/Tags/Event.ini) ──

	/** 경비견이 플레이어에 접촉했다는 사실. 3회 누적 판정은 플레이어 파트가 한다 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Guard_Contacted);

	/** 다운 조건 성립. GAB_Downed 의 GameplayEvent 트리거 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Player_Downed);

	/**
	 * 밴에 탔다. AVanZone 이 승차자의 ASC 로 보낸다.
	 *
	 * 내렸을 때는 보내지 않는다 — 이벤트는 "일어난 일" 이고, 승차 여부라는 상태는
	 * AHeistGameState 의 명단과 State.InVan 태그가 들고 있다.
	 *
	 * 페이로드 규약
	 *   Instigator      승차한 폰
	 *   Target          승차한 폰
	 *   EventMagnitude  이 승차로 밴에 탄 총 인원
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Player_BoardedVan);

	/**
	 * 밴 적재 확정. AVanLoadZone 이 적재자의 ASC 로 보낸다.
	 *
	 * 페이로드 규약 — 받는 쪽(기여도 집계 · HUD 팝업)이 이 약속에 기대므로 바꾸지 말 것.
	 *   Instigator      적재된 ALootBase
	 *   Target          적재자 폰
	 *   OptionalObject  적재된 ALootBase (BP 에서 캐스트해 쓰기 좋게 한 번 더)
	 *   EventMagnitude  이번에 더해진 금액($). 파손·유출이 반영된 GetCurrentValue() 다
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Loot_Loaded);

	/**
	 * 주 운반자가 중량형을 놓았다. ABaseCharacter::SetHeldActor 가 보조자의 ASC 로 보낸다.
	 *
	 * UGA_HeavyCarryAssist 의 감시 태스크가 다음 틱 폴링으로도 알아채지만, 그때까지
	 * 보조자가 허공을 잡고 있는 한 프레임이 남는다. 즉시 통지해 그 틈을 없앤다.
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Loot_Dropped);

	// Guard.Type.* 은 선언하지 않는다 — EGuardType 열거형과 두 벌이 된다 (AI/GuardTypes.h).
}
