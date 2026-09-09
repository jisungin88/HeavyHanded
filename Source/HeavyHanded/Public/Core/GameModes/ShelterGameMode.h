#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ShelterGameMode.generated.h"

enum class ENicknameError : uint8;

UCLASS()
class HEAVYHANDED_API AShelterGameMode : public AGameMode
{
	GENERATED_BODY()


public:

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	bool TryApplyNickname(APlayerController* Player, const FString& Raw, ENicknameError& OutError);

	virtual void ChangeName(AController* Controller, const FString& NewName, bool bNameChange) override;
};
