#pragma once

#include "CoreMinimal.h"
#include "HeistSpectateTypes.generated.h"

/** 관전자에게 보여줄 정보의 범위 */
UENUM(BlueprintType)
enum class EHeistSpectateInfoLevel : uint8
{
	/** 보고 있는 팀원이 보는 것만 */
	FollowTarget UMETA(DisplayName = "Follow Target"),

	/** 카메라만. HUD 정보 숨김 */
	ViewOnly     UMETA(DisplayName = "View Only"),
};
