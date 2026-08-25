#include "Core/HeistTravel.h"

FString HeistTravel::BuildTravelURL(const FSoftObjectPath& LevelPath, int32 ExpectedPlayers)
{
	// ServerTravel 은 패키지 이름을 받는다. 소프트 경로는 "/Game/.../L_Mansion.L_Mansion" 처럼
	// 에셋 이름까지 들고 있어서, 그대로 넘기면 맵을 찾지 못한다
	const FString PackageName = LevelPath.GetLongPackageName();

	if (PackageName.IsEmpty())
	{
		return FString();
	}

	if (ExpectedPlayers <= 0)
	{
		return PackageName;
	}

	// 옵션 이름은 AHeistGameMode::ResolveExpectedPlayers 의 GetIntOption 과 짝이다. 같이 고칠 것
	return FString::Printf(TEXT("%s?ExpectedPlayers=%d"), *PackageName, ExpectedPlayers);
}
