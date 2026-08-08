#include "UI/UISettings.h"

#include "UObject/Class.h"
#include "UObject/ReflectedTypeAccessors.h"

FLinearColor UUISettings::GetAlertLevelColor(EAlertLevel Level) const
{
	switch (Level)
	{
	case EAlertLevel::Calm:       return CalmColor;
	case EAlertLevel::Suspicious: return SuspiciousColor;
	case EAlertLevel::Alerted:    return AlertedColor;
	case EAlertLevel::Alarm:      return AlarmColor;
	}

	return CalmColor;
}

FText UUISettings::GetAlertLevelText(EAlertLevel Level)
{
	// EAlertLevel 의 UMETA(DisplayName) 이 이미 한글 이름을 갖고 있다.
	// 여기서 또 정의하면 enum 이 바뀔 때 조용히 어긋난다
	return StaticEnum<EAlertLevel>()->GetDisplayNameTextByValue(static_cast<int64>(Level));
}
