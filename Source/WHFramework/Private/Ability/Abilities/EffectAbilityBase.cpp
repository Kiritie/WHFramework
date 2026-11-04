#include "Ability/Abilities/EffectAbilityBase.h"

#include "Ability/Effects/EffectBase.h"
#include "AbilitySystemComponent.h"

UEffectAbilityBase::UEffectAbilityBase()
{
}

void UEffectAbilityBase::BeginAction_Implementation()
{
	EffectHandles.Reset();
	const FGuid ApplyingId = GetActionActivationId();
	TWeakObjectPtr<UAbilitySystemComponent> ApplyingSystem = GetAbilitySystemComponentFromActorInfo();
	for (TSubclassOf<UEffectBase> EffectClass : EffectClasses)
	{
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
		if (!Spec.IsValid())
		{
			FailAction(AbilityActionTags::InvalidRequest);
			return;
		}
		const FActiveGameplayEffectHandle Applied = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Spec);
		if (!IsActive() || GetActionActivationId() != ApplyingId)
		{
			if (ApplyingSystem.IsValid() && Applied.IsValid())
			{
				ApplyingSystem->RemoveActiveGameplayEffect(Applied);
			}
			return;
		}
		if (!Applied.WasSuccessfullyApplied())
		{
			FailAction(AbilityActionTags::ActivationFailed);
			return;
		}
		if (Applied.IsValid())
		{
			EffectHandles.Add(Applied);
		}
	}
	CompleteAction();
}

void UEffectAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive() || bCleaningEffects)
	{
		return;
	}
	bCleaningEffects = true;
	const FGuid EndingId = GetActionActivationId();
	const TArray<FActiveGameplayEffectHandle> Applied = MoveTemp(EffectHandles);
	if (bWasCancelled)
	{
		for (FActiveGameplayEffectHandle Effect : Applied)
		{
			BP_RemoveGameplayEffectFromOwnerWithHandle(Effect);
		}
	}
	bCleaningEffects = false;
	if (IsActive() && GetActionActivationId() == EndingId)
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

TArray<TSubclassOf<UEffectBase>> UEffectAbilityBase::GetEffectClasses() const
{
	TArray<TSubclassOf<UEffectBase>> Result = EffectClasses;
	Result.Append(Super::GetEffectClasses());
	return Result;
}
