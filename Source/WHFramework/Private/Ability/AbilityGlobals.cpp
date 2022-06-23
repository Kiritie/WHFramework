// Copyright 2020 Dan Kestranek.


#include "Ability/AbilityGlobals.h"

UAbilityGlobals::UAbilityGlobals()
{

}

FGameplayEffectContext* UAbilityGlobals::AllocGameplayEffectContext() const
{
	return new FGameplayEffectContext();
}

void UAbilityGlobals::InitGlobalTags()
{
	Super::InitGlobalTags();

	DeadTag = FGameplayTag::RequestGameplayTag("State.Dead");
	KnockedDownTag = FGameplayTag::RequestGameplayTag("State.KnockedDown");
	InteractingTag = FGameplayTag::RequestGameplayTag("State.Interacting");
	InteractingRemovalTag = FGameplayTag::RequestGameplayTag("State.InteractingRemoval");
}
