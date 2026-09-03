#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "CharacterSettings.generated.h"

class APawn;

USTRUCT()
struct FHeistRolePawn
{
	GENERATED_BODY()

	/** 역할 식별자 */
	UPROPERTY(EditAnywhere, Category = "Role")
	FGameplayTag RoleTag;

	/** 스폰되는 폰 */
	UPROPERTY(EditAnywhere, Category = "Role", meta = (AllowedClasses = "/Script/Engine.Pawn"))
	TSoftClassPtr<APawn> PawnClass;
};

UCLASS(config = Character, defaultconfig, meta = (DisplayName = "Character"))
class HEAVYHANDED_API UCharacterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UCharacterSettings* Get() { return GetDefault<UCharacterSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 역할 선택 */
	UPROPERTY(config, EditAnywhere, Category = "Role", meta = (TitleProperty = "RoleTag"))
	TArray<FHeistRolePawn> RolePawnClasses;

	/** 역할 미선택 */
	UPROPERTY(config, EditAnywhere, Category = "Role", meta = (AllowedClasses = "/Script/Engine.Pawn"))
	TSoftClassPtr<APawn> DefaultRolePawnClass;

	UClass* ResolveRolePawnClass(const FGameplayTag& RoleTag) const;
};
