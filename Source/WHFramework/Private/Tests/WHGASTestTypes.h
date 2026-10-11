#pragma once

#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Effects/EffectBase.h"
#include "WHGASTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class UWHGASTestAbility : public UAbilityBase
{
	GENERATED_BODY()

public:
	UWHGASTestAbility();
};

UCLASS(Transient, NotBlueprintable)
class UWHGASTestAttributeEffect : public UEffectBase
{
	GENERATED_BODY()

public:
	UWHGASTestAttributeEffect();
};

UCLASS(Transient, NotBlueprintable)
class UWHGASTestInstantEffect : public UWHGASTestAttributeEffect
{
	GENERATED_BODY()

public:
	UWHGASTestInstantEffect();
};
