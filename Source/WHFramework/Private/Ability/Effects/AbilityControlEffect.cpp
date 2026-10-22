#include "Ability/Effects/AbilityControlEffect.h"

UAbilityControlEffect::UAbilityControlEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	bPersistAcrossSave = false;
	bRemoveOnDeath = true;
}

UAbilityTimedControlEffect::UAbilityTimedControlEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f);
}
