// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "PlayerSessionState.generated.h"

class UAbilitySystemComponent;
class UBaseAttributeSet;

/**
 * 
 */
UCLASS()
class HEAVYHANDED_API APlayerSessionState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
    APlayerSessionState();

    // GAS 인터페이스 구현 (플레이어가 세션 레벨/스탯을 가지므로 여기서 ASC를 제공)
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    FORCEINLINE UBaseAttributeSet* GetBaseAttributeSet() const { return BaseAttributeSet; }

    // [필수] 플레이어 세션 데이터 Getter / Setter
    UFUNCTION(BlueprintCallable, Category = "PlayerSession|Data")
    int32 GetGold() const { return Gold; }

    UFUNCTION(BlueprintCallable, Category = "PlayerSession|Data")
    void AddGold(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "PlayerSession|Data")
    int32 GetSelectedCharacterID() const { return SelectedCharacterID; }

    UFUNCTION(BlueprintCallable, Category = "PlayerSession|Data")
    void SetSelectedCharacterID(int32 NewCharacterID);

protected:
    // 네트워크 동기화 변수 등록 (Replication)
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- GAS 컴포넌트 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBaseAttributeSet> BaseAttributeSet;

    // --- 핵심 세션 데이터 (멀티플레이 동기화 적용) ---

    // 플레이어 소유 주화 (골드)
    UPROPERTY(ReplicatedUsing = OnRep_Gold, BlueprintReadOnly, Category = "PlayerSession|Data")
    int32 Gold;

    UFUNCTION()
    virtual void OnRep_Gold();

    // 로비에서 선택한 캐릭터 고유 번호 (또는 타입)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "PlayerSession|Data")
    int32 SelectedCharacterID;
};
