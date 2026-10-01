// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTService_SetLookAround.h"

#include "AIController.h"
#include "AI/GuardAnimInstance.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UBTService_SetLookAround::UBTService_SetLookAround()
{
	NodeName = TEXT("Set Look Around");

	// 이게 있어야 시작/종료 감지 가능
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;

}

void UBTService_SetLookAround::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	// 현재 Behavior Tree를 실행하고 있는 AI Controller를 가져온다.
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!IsValid(AIController))
	{
		return;
	}

	// 현재 AI Controller가 빙의하고 있는 경비 캐릭터를 가져온다.
	ACharacter* GuardCharacter = Cast<ACharacter>(AIController->GetPawn());
	if (!IsValid(GuardCharacter))
	{
		return;
	}

	// 경비 캐릭터의 Skeletal Mesh에서 현재 사용 중인 AnimInstance를 가져온다.
	UGuardAnimInstance* AnimInstance = Cast<UGuardAnimInstance>(GuardCharacter->GetMesh()->GetAnimInstance());
	if (!IsValid(AnimInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LookAround] GuardAnimInstance 캐스팅 실패 | Pawn=%s"),
			*GetNameSafe(GuardCharacter));
		return;
	}

	// Wait 서비스 인스턴스에 설정된 두리번 애니메이션 종류를 적용한다.
	AnimInstance->SetLookAroundType(LookAroundType);

	//UE_LOG(LogTemp, Warning, TEXT("[LookAround] Type=%d | Pawn=%s"),
		//static_cast<int32>(LookAroundType), *GetNameSafe(GuardCharacter));

}

void UBTService_SetLookAround::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	// 현재 Behavior Tree를 실행하고 있는 AI Controller를 가져온다.
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!IsValid(AIController))
	{
		return;
	}

	// 현재 AI Controller가 빙의하고 있는 경비 캐릭터를 가져온다.
	ACharacter* GuardCharacter = Cast<ACharacter>(AIController->GetPawn());
	if (!IsValid(GuardCharacter))
	{
		return;
	}

	// 경비 캐릭터의 Skeletal Mesh에서 현재 사용 중인 AnimInstance를 가져온다.
	UGuardAnimInstance* AnimInstance = Cast<UGuardAnimInstance>(GuardCharacter->GetMesh()->GetAnimInstance());
	if (!IsValid(AnimInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LookAround] GuardAnimInstance 캐스팅 실패 | Pawn=%s"),
			*GetNameSafe(GuardCharacter));
		return;
	}

	// Wait 서비스가 종료되거나 Abort되었으므로 두리번 애니메이션 상태를 초기화한다.
	AnimInstance->SetLookAroundType(EGuardLookAroundType::None);

	UE_LOG(LogTemp, Warning, TEXT("[LookAround] Type=None | Pawn=%s"),
		*GetNameSafe(GuardCharacter));

}
