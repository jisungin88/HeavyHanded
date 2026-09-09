// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BaseAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API UBaseAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
    UBaseAttributeSet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 체력
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UBaseAttributeSet, Health)

    // 이동 속도
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MovementSpeed)
    FGameplayAttributeData MovementSpeed;
    ATTRIBUTE_ACCESSORS(UBaseAttributeSet, MovementSpeed)

    // 스태미나 — 기획서 근거 없음, 임의값(초기 100). 스프린트 소모는 GE_Sprint(BP)의
    // Periodic 모디파이어가 담당하고, 여기서는 클램프만 한다.
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Stamina)
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(UBaseAttributeSet, Stamina)

    // 스태미나 최대치 — 기획서 근거 없음, 임의값(100)
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(UBaseAttributeSet, MaxStamina)

    UFUNCTION()
    virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

    UFUNCTION()
    virtual void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed);

    UFUNCTION()
    virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

    UFUNCTION()
    virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

    // Stamina 는 [0, MaxStamina] 를 벗어나면 안 된다 — Duration 모디파이어의 CurrentValue 클램프.
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    // GE_Sprint 의 Periodic 모디파이어는 BaseValue 를 직접 깎으므로, PreAttributeChange 만으로는
    // 클램프가 보장되지 않는다 — 매 적용 직후 BaseValue 를 한 번 더 눌러준다.
    virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
};
