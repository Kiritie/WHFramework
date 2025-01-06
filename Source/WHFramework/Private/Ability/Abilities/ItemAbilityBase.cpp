#include "Ability/Abilities/ItemAbilityBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/Effects/EffectBase.h"

UItemAbilityBase::UItemAbilityBase()
{
}

void UItemAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	for(auto& Iter : EffectClasses)
	{
		const FGameplayEffectSpecHandle EffectSpec = MakeOutgoingGameplayEffectSpec(Iter, GetCurrentAbilitySpec()->Level);
		EffectHandles.Add(ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, EffectSpec));
	}
}

void UItemAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
	if(bWasCancelled)
	{
		for(auto& Iter : EffectHandles)
		{
			BP_RemoveGameplayEffectFromOwnerWithHandle(Iter);
		}
	}
}

TArray<TSubclassOf<UEffectBase>> UItemAbilityBase::GetEffectClasses() const
{
	TArray<TSubclassOf<UEffectBase>> ReturnValues;
	ReturnValues.Append(EffectClasses);
	ReturnValues.Append(Super::GetEffectClasses());
	return ReturnValues;
}
