#pragma once

#include "Ability/Effects/EffectBase.h"
#include "AbilityControlEffect.generated.h"

UCLASS()
class WHFRAMEWORK_API UAbilityControlEffect : public UEffectBase
{
	GENERATED_BODY()

public:
	UAbilityControlEffect();
};

UCLASS()
class WHFRAMEWORK_API UAbilityTimedControlEffect : public UAbilityControlEffect
{
	GENERATED_BODY()

public:
	UAbilityTimedControlEffect();
};
