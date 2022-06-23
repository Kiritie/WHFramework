
#include "Ability/Abilities/VitalityAbilityBase.h"

#include "Ability/Vitality/AbilityVitalityBase.h"

UVitalityAbilityBase::UVitalityAbilityBase()
{
	
}

AAbilityVitalityBase* UVitalityAbilityBase::GetOwnerVitality() const
{
	return Cast<AAbilityVitalityBase>(GetOwningActorFromActorInfo());
}
