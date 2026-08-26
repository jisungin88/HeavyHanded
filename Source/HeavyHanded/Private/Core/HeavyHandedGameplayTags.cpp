#include "Core/HeavyHandedGameplayTags.h"

// 문자열은 짝이 되는 Config/Tags/<시스템>.ini 의 정의와 한 글자도 다르면 안 된다.
// 여기서 정의한 태그는 네이티브로 등록되므로, .ini 에서 사라져도 태그 자체는 살아남는다.
// 선언과 소유 구분은 Public/Core/HeavyHandedGameplayTags.h 에 있다.
namespace HHTags
{
	UE_DEFINE_GAMEPLAY_TAG(Noise_Loot_Impact, "Noise.Loot.Impact");
	UE_DEFINE_GAMEPLAY_TAG(Noise_Loot_Break,  "Noise.Loot.Break");

	UE_DEFINE_GAMEPLAY_TAG(Phase_Prep,   "Phase.Prep");
	UE_DEFINE_GAMEPLAY_TAG(Phase_Heist,  "Phase.Heist");
	UE_DEFINE_GAMEPLAY_TAG(Phase_Escape, "Phase.Escape");
	UE_DEFINE_GAMEPLAY_TAG(Phase_Result, "Phase.Result");

	UE_DEFINE_GAMEPLAY_TAG(Loot_Type,          "Loot.Type");
	UE_DEFINE_GAMEPLAY_TAG(Loot_Type_Heavy,    "Loot.Type.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Loot_Type_Fragile,  "Loot.Type.Fragile");
	UE_DEFINE_GAMEPLAY_TAG(Loot_Type_Unstable, "Loot.Type.Unstable");

	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Idle,    "Loot.State.Idle");
	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Carried, "Loot.State.Carried");
	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Dropped, "Loot.State.Dropped");
	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Broken,  "Loot.State.Broken");
	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Loaded,  "Loot.State.Loaded");

	UE_DEFINE_GAMEPLAY_TAG(State_Downed,       "State.Downed");
	UE_DEFINE_GAMEPLAY_TAG(State_InVan,        "State.InVan");
	UE_DEFINE_GAMEPLAY_TAG(State_Arrested,     "State.Arrested");
	UE_DEFINE_GAMEPLAY_TAG(State_Sprinting,    "State.Sprinting");

	UE_DEFINE_GAMEPLAY_TAG(Ability_HeavyCarryAssist, "Ability.HeavyCarryAssist");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Slot_Consumable,  "Ability.Slot.Consumable");

	UE_DEFINE_GAMEPLAY_TAG(Event_Guard_Contacted,   "Event.Guard.Contacted");
	UE_DEFINE_GAMEPLAY_TAG(Event_Player_Downed,     "Event.Player.Downed");
	UE_DEFINE_GAMEPLAY_TAG(Event_Player_BoardedVan, "Event.Player.BoardedVan");
	UE_DEFINE_GAMEPLAY_TAG(Event_Loot_Loaded,       "Event.Loot.Loaded");
	UE_DEFINE_GAMEPLAY_TAG(Event_Loot_Dropped,      "Event.Loot.Dropped");

	UE_DEFINE_GAMEPLAY_TAG(Equipment_StickyBomb, "Equipment.StickyBomb");
}
