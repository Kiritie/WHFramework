#include "Ability/Effects/CharacterStateEffectBase.h"

UCharacterStateEffectBase::UCharacterStateEffectBase()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

UCharacterTimedStateEffectBase::UCharacterTimedStateEffectBase()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f);
}

namespace CharacterLifecycleTags
{
	UE_DEFINE_GAMEPLAY_TAG(Suspended, "State.Character.Suspended");
}
