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

    // 골드는 여기 없다.
    //
    //   노획한 금액은 개인 지갑이 아니라 팀 공용 지갑으로 들어간다. 개인 골드를 PlayerState 에
    //   두면 두 가지가 동시에 깨진다.
    //     1. 소유 주체가 틀린다 — 공용 잔액을 인원수만큼 복사해 두고 서로 맞추게 된다.
    //     2. 레벨을 못 넘는다 — 비-심리스 ServerTravel 은 PlayerState 를 파괴한다.
    //        은신처에서 장비를 사고 저택에 도착하면 잔액이 0 으로 돌아간다.
    //
    //   URunProgressSubsystem::GetTeamGold() 를 쓸 것. 화면에 그릴 값이면 GameState 로 복제된다.

    // [필수] 플레이어 세션 데이터 Getter / Setter
    /*UFUNCTION(BlueprintCallable, Category = "PlayerSession|Data")
    int32 GetSelectedCharacterID() const { return SelectedCharacterID; }

    UFUNCTION(BlueprintCallable, Category = "PlayerSession|Data")
    void SetSelectedCharacterID(int32 NewCharacterID);*/

protected:
    // 네트워크 동기화 변수 등록 (Replication)
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- GAS 컴포넌트 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBaseAttributeSet> BaseAttributeSet;

    // --- 핵심 세션 데이터 (멀티플레이 동기화 적용) ---

    // 로비에서 선택한 캐릭터 고유 번호 (또는 타입)
    /*UPROPERTY(Replicated, BlueprintReadOnly, Category = "PlayerSession|Data")
    int32 SelectedCharacterID;*/
};
