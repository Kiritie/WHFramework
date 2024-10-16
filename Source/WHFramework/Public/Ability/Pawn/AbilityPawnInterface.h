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
	virtual void Static() = 0;

	virtual void UnStatic() = 0;

public:
	virtual bool IsActive(bool bNeedNotDead = false) const = 0;

	virtual bool IsMoving() const = 0;

	virtual bool IsPlayer() const = 0;
	
	virtual bool IsEnemy(IAbilityPawnInterface* InTarget) const = 0;
};
