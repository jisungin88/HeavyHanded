// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Ability/GAB_Throw.h"
#include "Character/BaseCharacter.h"
#include "Interfaces/Carryable.h"
#include "Core/HeavyHandedGameplayTags.h"   // Ability.Slot.Consumable 네이티브 태그
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"

UGAB_Throw::UGAB_Throw()
{
	bReplicateInputDirectly = true;
}

bool UGAB_Throw::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 일반 노획물을 들고 있을 때만 던지기가 성립한다.
	// 안 들었거나(빈손) 중량형(2인 필수, Loot.ini — 던지기 불가)을 들었으면
	// 여기서 막아 몽타주 태스크 자체가 생성되지 않게 한다.
	const ABaseCharacter* Character = ActorInfo ? Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character || !Character->GetHeldActor() || Character->IsCarryingHeavyItem())
	{
		return false;
	}

	return true;
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
	// 여기까지 왔다면 정상적으로는 CanActivateAbility 를 이미 통과한 상태다.
	// 그래도 활성화 시점과 이 사이 한 프레임 안에 상태가 바뀔 수 있으므로
	// (예: 서버 권위 판정이 늦게 도착) 방어적으로 다시 확인한다.
	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character || !Character->GetHeldActor() || Character->IsCarryingHeavyItem())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bThrowExecuted = false;

	// 몽타주가 실제로 재생 중일 때만 hold 루프를 걸고 Notify 를 구독한다.
	if (IsPlayingSkillMontage())
	{
		BeginHoldLoop();

		if (UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UGAB_Throw::OnMontageNotifyBegin);
		}
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

	// 조준선은 손을 뗀 순간 지운다.
	ClearTrajectoryPreview();

	// Hold 루프를 빠져나와 HoldEnd(마지막 던지는 모션)로 진행시킨다.
	// 실제 던지기는 여기서 하지 않는다 — HoldEnd 안에 찍힌 Notify(OnMontageNotifyBegin)가
	// 물체가 실제로 손을 떠나는 프레임에 ExecuteThrow() 를 실행한다.
	ReleaseHoldLoop();

	// 클라이언트라면 서버 쪽 어빌리티 인스턴스에도 같은 InputReleased 를 알린다.
	// (몽타주/Notify 흐름은 서버·클라 각자 자기 쪽에서 진행되므로 여기서 EndAbility 하지 않는다 —
	//  몽타주가 끝나야 OnMontageCompleted/OnMontageBlendOut 에서 어빌리티가 끝난다)
	if (!ActorInfo->IsNetAuthority())
	{
		FGameplayEventData Payload;
		SendGameplayEventToASCOnServer(HHTags::Ability_Slot_Consumable, Payload);
	}
}

void UGAB_Throw::BeginHoldLoop()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		// HoldStart 구간을 자기 자신으로 되돌려서, 마우스 유지 중엔 여기서 정지된 것처럼 반복.
		ASC->CurrentMontageSetNextSectionName(HoldStartSectionName, HoldStartSectionName);
	}
}

void UGAB_Throw::ReleaseHoldLoop()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		// 다음 루프 지점에서 HoldStart 로 안 돌아가고 HoldEnd(던지는 모션)로 진행.
		ASC->CurrentMontageSetNextSectionName(HoldStartSectionName, HoldEndSectionName);

		// HoldEnd 자신은 hold 루프를 만들기 위해 몽타주 에셋에서 HoldStart로 되돌아가도록
		// authored 되어 있다. 그 복귀 링크도 끊어야 HoldEnd가 되돌아가지 않고
		// 몽타주 끝까지 자연스럽게 흘러간다. (안 끊으면 HoldStart<->HoldEnd 핑퐁에 갇힘)
		ASC->CurrentMontageSetNextSectionName(HoldEndSectionName, NAME_None);
	}
}

void UGAB_Throw::OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (NotifyName != ThrowReleaseNotifyName)
	{
		return;
	}

	// 같은 메시에서 재생 중인 다른 몽타주의 동명 Notify 오탐 방지.
	if (BranchingPointPayload.SequenceAsset != SkillMontage)
	{
		return;
	}

	// 던지기(물리/아이템 위임)는 서버 권한에서만 실행한다.
	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		if (ActorInfo->IsNetAuthority())
		{
			ExecuteThrow();
		}
	}
}

void UGAB_Throw::ExecuteThrow()
{
	if (bThrowExecuted)
	{
		return;
	}
	bThrowExecuted = true;

	ABaseCharacter* Character = GetBaseCharacterFromActorInfo();
	if (!Character)
	{
		return;
	}

	if (AActor* HeldActor = Character->GetHeldActor())
	{
		// ICarryable(= 노획물)은 던지기를 자기가 처리한다.
		//
		// 세기·포물선·회전·클리어런스가 전부 아이템 데이터(FLootPhysicsData)에서 나오고,
		// 임펄스에 질량을 곱하기 때문에 무게가 달라도 ThrowSpeed 그대로 날아간다.
		// 중량형처럼 던질 수 없는 물건을 제자리에 놓는 판정도 아이템 몫이다.
		// (플레이어는 요청하고, 아이템이 허용/거부한다 — ICarryable 주석)
		//
		// 여기서 임펄스를 직접 주면 놓기 임펄스와 겹쳐 두 번 들어가고,
		// 착지 충격도 Throw 가 아니라 Drop 으로 기록된다.
		if (ICarryable* Carryable = Cast<ICarryable>(HeldActor))
		{
			// 조준 방향은 아직 들려 있을 때만 구할 수 있다. 놓은 뒤에는 영벡터가 나온다.
			const FVector AimDirection = Carryable->ComputeThrowAimDirection();

			// 디태치 · 물리 ON · 임펄스 · 회전까지 전부 여기서 끝난다.
			Carryable->OnThrown(Character, AimDirection);

			// 위에서 이미 손을 떠났으므로 이 호출은 참조 정리만 한다.
			// (OnReleased 는 PrimaryCarrier 가 비어 있어 조용히 무시된다)
			Character->SetHeldActor(nullptr);
		}
		else
		{
			// ICarryable 을 구현하지 않은 액터용 폴백. 테스트 맵의 "Item" 태그 액터 등.
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
		}

		// 던지자마자 다시 낚아채는 것을 잠깐 막는다. 던진 본인에게만 적용되므로
		// 동료에게 패스하는 것은 그대로 가능하다.
		Character->BlockRecatch(HeldActor);
	}
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

	// InstancedPerActor 라 인스턴스가 재사용된다. Notify 구독이 안 끊기면 다음 재생 때 중복 호출된다.
	if (ABaseCharacter* Character = GetBaseCharacterFromActorInfo())
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UGAB_Throw::OnMontageNotifyBegin);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}