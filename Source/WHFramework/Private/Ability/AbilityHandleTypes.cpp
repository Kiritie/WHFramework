#include "Ability/AbilityHandleTypes.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"
#include "GameFramework/Actor.h"

void UDamageHandle::HandleDamage(AActor* SourceActor, AActor* TargetActor, float DamageValue, const FGameplayAttribute& DamageAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	if (!IsValid(TargetActor) || !TargetActor->HasAuthority())
	{
		return;
	}

	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (DamageValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleDamage(DamageAttribute, DamageValue, false, false, HitResult, SourceTags, SourceActor);
		}
	}
}

void URecoveryHandle::HandleRecovery(AActor* SourceActor, AActor* TargetActor, float RecoveryValue, const FGameplayAttribute& RecoveryAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	if (!IsValid(TargetActor) || !TargetActor->HasAuthority())
	{
		return;
	}

	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (RecoveryValue > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleRecovery(RecoveryAttribute, RecoveryValue, HitResult, SourceTags, SourceActor);
		}
	}
}

void UInterruptHandle::HandleInterrupt(AActor* SourceActor, AActor* TargetActor, float InterruptDuration, const FGameplayAttribute& InterruptAttribute, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	if (!IsValid(TargetActor) || !TargetActor->HasAuthority())
	{
		return;
	}

	IAbilityVitalityInterface* TargetVitality = Cast<IAbilityVitalityInterface>(TargetActor);

	if (InterruptDuration > 0.f)
	{
		if (TargetVitality && !TargetVitality->IsDead())
		{
			TargetVitality->HandleInterrupt(InterruptAttribute, InterruptDuration, HitResult, SourceTags, SourceActor);
		}
	}
}
