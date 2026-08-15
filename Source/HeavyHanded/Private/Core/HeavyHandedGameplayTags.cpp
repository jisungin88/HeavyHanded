#include "Core/HeavyHandedGameplayTags.h"

// 문자열은 Config/Tags/Noise.ini 의 정의와 한 글자도 다르면 안 된다.
// 여기서 정의한 태그는 네이티브로 등록되므로, .ini 에서 사라져도 태그 자체는 살아남는다.
namespace HHTags
{
	UE_DEFINE_GAMEPLAY_TAG(Noise_Loot_Impact, "Noise.Loot.Impact");
	UE_DEFINE_GAMEPLAY_TAG(Noise_Loot_Break,  "Noise.Loot.Break");

	UE_DEFINE_GAMEPLAY_TAG(Loot_Type,          "Loot.Type");
	UE_DEFINE_GAMEPLAY_TAG(Loot_Type_Heavy,    "Loot.Type.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Loot_Type_Fragile,  "Loot.Type.Fragile");
	UE_DEFINE_GAMEPLAY_TAG(Loot_Type_Unstable, "Loot.Type.Unstable");

	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Idle,    "Loot.State.Idle");
	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Carried, "Loot.State.Carried");
	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Dropped, "Loot.State.Dropped");
	UE_DEFINE_GAMEPLAY_TAG(Loot_State_Broken,  "Loot.State.Broken");
}
