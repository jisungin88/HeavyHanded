#include "Hazards/HazardBase.h"

#include "AbilitySystemComponent.h"
#include "Character/BaseCharacter.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"   // FGameplayTag::RequestGameplayTag — State.ShadowStep 임시 문자열 조회
#include "Kismet/GameplayStatics.h"

bool AHazardBase::IsValidHazardTarget(AActor* OtherActor, ABaseCharacter*& OutTarget, bool bCheckShadowStep) const
{
	OutTarget = Cast<ABaseCharacter>(OtherActor);
	if (!IsValid(OutTarget))
	{
		// AGuardCharacter 는 별도 클래스라 여기 안 걸린다 — 경비가 자기 구역 함정에 스스로 안 걸리는 이유
		return false;
	}

	if (bCheckShadowStep)
	{
		// [임시: 네이티브 선언 대신 문자열 조회] — HeavyHandedGameplayTags.h 를 건드리지 않는다
		if (UAbilitySystemComponent* ASC = OutTarget->GetAbilitySystemComponent())
		{
			static const FGameplayTag ShadowStepTag = FGameplayTag::RequestGameplayTag(TEXT("State.ShadowStep"));
			if (ASC->HasMatchingGameplayTag(ShadowStepTag))
			{
				return false;
			}
		}
	}

	return true;
}

bool AHazardBase::ShouldRetrigger(float Interval)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (LastTriggerTime >= 0.f && Now - LastTriggerTime < Interval)
	{
		// 오버랩 경계에서 스치듯 들락거린 것 — 진짜 재진입이 아니다
		return false;
	}

	LastTriggerTime = Now;
	return true;
}

void AHazardBase::PlayHazardSound(USoundBase* Sound) const
{
	const UWorld* World = GetWorld();

	// 데디케이티드 서버는 화면도 스피커도 없다
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (IsValid(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, GetActorLocation());
	}
}
