// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Ability/GAB_Throw.h"
#include "Character/BaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UGAB_Throw::UGAB_Throw()
{
	bReplicateInputDirectly = true;
}

void UGAB_Throw::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 1. 부모의 ActivateAbility 호출 (몽타주 재생 로직이 여기에 있음)
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Commit 실패나 몽타주 재생 실패로 Super 에서 이미 종료됐으면 더 진행하지 않는다.
	// 이 어빌리티는 InputReleased 를 기다리므로, 끝난 상태에서 타이머만 남으면
	// 궤적만 그려지고 던지기는 영영 실행되지 않는다.
	if (!IsActive())
	{
		return;
	}

	// 2. 캐릭터 확인
	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character || !Character->GetHeldActor())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 꾹 누르고 있는 동안 궤적을 실시간으로 그려줄 타이머 시작.
	// 미리보기는 조준하는 본인 화면에만 필요하므로, 데디케이티드 서버와
	// 원격 프록시에서는 타이머 자체를 돌리지 않는다.
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() != NM_DedicatedServer && Character->IsLocallyControlled())
	{
		World->GetTimerManager().SetTimer(
			TrajectoryUpdateTimerHandle,
			this,
			&UGAB_Throw::UpdateTrajectoryPreview,
			TrajectoryUpdateInterval,
			true
		);
	}
}

void UGAB_Throw::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	// 차징 시각화 및 타이머 정리
	ClearTrajectoryPreview();

	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 1. 클라이언트라면 부모의 헬퍼를 통해 서버로 이벤트 전송
	if (!ActorInfo->IsNetAuthority())
	{
		FGameplayEventData Payload;
		FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("Ability.Slot.Consumable"));
		SendGameplayEventToASCOnServer(EventTag, Payload);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 2. 서버 권한에서의 실제 던지기 (물리 부여)
	if (AActor* HeldActor = Character->GetHeldActor())
	{
		// 들고 있는 상태 해제. 분리와 물리/콜리전 복구까지 여기서 처리된다.
		Character->SetHeldActor(nullptr);

		// 물리가 켜진 뒤에야 임펄스가 먹는다 — 순서를 바꾸지 말 것
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(HeldActor->GetRootComponent()))
		{
			const FVector LaunchVelocity = Character->GetActorForwardVector() * ThrowSpeed;
			if (!LaunchVelocity.IsNearlyZero())
			{
				PrimComp->AddImpulse(LaunchVelocity, NAME_None, true);
			}
		}

		// 던지자마자 다시 낚아채는 것을 잠깐 막는다. 던진 본인에게만 적용되므로
		// 동료에게 패스하는 것은 그대로 가능하다.
		Character->BlockRecatch(HeldActor);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGAB_Throw::UpdateTrajectoryPreview()
{
#if ENABLE_DRAW_DEBUG
	// [디버그 전용] 던지기 궤적 시각화. hh.Ability.Debug 2
	// 꺼져 있으면 경로 예측 계산까지 통째로 건너뛴다.
	if (CVarAbilityDebug.GetValueOnGameThread() < 2)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	const FVector StartLocation = Character->GetActorLocation() + MuzzleOffset;
	const FVector LaunchVelocity = Character->GetActorForwardVector() * ThrowSpeed;

	FPredictProjectilePathParams Params(
		ProjectileRadius,
		StartLocation,
		LaunchVelocity,
		2.0f,
		ECollisionChannel::ECC_WorldStatic,
		Character
	);
	Params.bTraceWithCollision = true;
	Params.SimFrequency = 15.0f;

	// 자동 디버그 드로우는 끄고, 직접 선을 그릴 것이므로 None
	Params.DrawDebugType = EDrawDebugTrace::None;

	FPredictProjectilePathResult Result;
	UGameplayStatics::PredictProjectilePath(this, Params, Result);

	UWorld* World = GetWorld();
	if (!World || Result.PathData.Num() < 2)
	{
		return;
	}

	// 경로 포인트들을 순서대로 이어서 매끈한 선으로 그림
	for (int32 i = 0; i < Result.PathData.Num() - 1; ++i)
	{
		const FVector& PointA = Result.PathData[i].Location;
		const FVector& PointB = Result.PathData[i + 1].Location;

		DrawDebugLine(World, PointA, PointB, FColor::Yellow, false, TrajectoryUpdateInterval, 0, 2.0f);
	}

	// 뭔가에 부딪혔다면 그 착탄 지점에 표시
	if (Result.HitResult.bBlockingHit)
	{
		DrawDebugSphere(World, Result.HitResult.Location, 12.0f, 12, FColor::Red, false, TrajectoryUpdateInterval);
	}
#endif   // ENABLE_DRAW_DEBUG
}

void UGAB_Throw::ClearTrajectoryPreview()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TrajectoryUpdateTimerHandle);
	}
}

void UGAB_Throw::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 스킬이 어떤 이유로든 끝날 때 타이머와 그려진 궤적이 남지 않도록 확실하게 정리
	ClearTrajectoryPreview();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}