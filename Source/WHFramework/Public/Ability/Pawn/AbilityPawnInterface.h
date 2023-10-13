#pragma once

#include "Ability/Vitality/AbilityVitalityInterface.h"

#include "AbilityPawnInterface.generated.h"

class UAbilityBase;
UINTERFACE()
class WHFRAMEWORK_API UAbilityPawnInterface : public UAbilityVitalityInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityPawnInterface : public IAbilityVitalityInterface
{
	GENERATED_BODY()

public:
	virtual bool IsPlayer() const = 0;
	
	virtual bool IsEnemy(IAbilityPawnInterface* InTarget) const = 0;
};
