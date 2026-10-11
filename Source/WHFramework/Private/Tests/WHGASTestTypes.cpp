#include "Tests/WHGASTestTypes.h"

#include "Ability/Attributes/VitalityAttributeSetBase.h"

UWHGASTestAbility::UWHGASTestAbility()
{
	ActionTag = AbilityActionTags::Request;
}

UWHGASTestAttributeEffect::UWHGASTestAttributeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	StackingType = EGameplayEffectStackingType::None;
	bPersistAcrossSave = true;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UVitalityAttributeSetBase::GetHealthRegenSpeedAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.f));
	Modifiers.Add(Modifier);
}

UWHGASTestInstantEffect::UWHGASTestInstantEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
}
