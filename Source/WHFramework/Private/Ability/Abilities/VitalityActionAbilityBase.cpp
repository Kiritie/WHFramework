#include "Ability/Abilities/VitalityActionAbilityBase.h"

#include "Ability/Vitality/AbilityVitalityInterface.h"

UVitalityActionAbilityBase::UVitalityActionAbilityBase()
{
	bWasStopped = false;
}

void UVitalityActionAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bWasStopped = false;
}

void UVitalityActionAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	IAbilityVitalityInterface* AbilityVitality = GetOwnerActor<IAbilityVitalityInterface>();

	if(!AbilityVitality) return;
	
	if(!bWasStopped)
	{
		AbilityVitality->EndAction(AbilityTags.GetByIndex(0), bWasCancelled);
	}
}
