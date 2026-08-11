// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/PlayerSessionState.h"
#include "Character/BaseAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

APlayerSessionState::APlayerSessionState()
{
    // GAS 컴포넌트 생성 및 멀티플레이 환경 Mixed 모드 설정
    AbilitySystemComp = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComp->SetIsReplicated(true);
    AbilitySystemComp->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // 레벨 등의 속성을 담을 AttributeSet 생성
    BaseAttributeSet = CreateDefaultSubobject<UBaseAttributeSet>(TEXT("BaseAttributeSet"));

    // 초기값 설정
    Gold = 0;
    SelectedCharacterID = 0;

    // 네트워크 업데이트 빈도 설정 (원활한 동기화)
    NetUpdateFrequency = 100.0f;
}

UAbilitySystemComponent* APlayerSessionState::GetAbilitySystemComponent() const
{
    return AbilitySystemComp;
}

// 변수 네트워크 동기화 설정 (서버 -> 클라이언트 자동 복제)
void APlayerSessionState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APlayerSessionState, Gold);
    DOREPLIFETIME(APlayerSessionState, SelectedCharacterID);
}

void APlayerSessionState::AddGold(int32 Amount)
{
    // 세션 데이터 변경은 서버에서만 권한을 가짐
    if (HasAuthority())
    {
        Gold += Amount;
        // 싱글플레이나 서버 화면에서는 OnRep이 자동 호출되지 않으므로 필요시 UI 갱신 바인딩 처리 추가 가능
    }
}

void APlayerSessionState::SetSelectedCharacterID(int32 NewCharacterID)
{
    if (HasAuthority())
    {
        SelectedCharacterID = NewCharacterID;
    }
}

void APlayerSessionState::OnRep_Gold()
{
    // 클라이언트에서 골드가 동기화되었을 때 필요한 처리 (UI 갱신 등)를 여기서 수행할 수 있습니다.
}
