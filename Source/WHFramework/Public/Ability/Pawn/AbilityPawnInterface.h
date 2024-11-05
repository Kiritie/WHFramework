#pragma once

#include "Ability/Vitality/AbilityVitalityInterface.h"

#include "AbilityPawnInterface.generated.h"

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

	virtual bool IsMoving() const = 0;

public:
	virtual void GetMotionRate(float& OutMovementRate, float& OutRotationRate) = 0;

	virtual void SetMotionRate(float InMovementRate, float InRotationRate) = 0;
};
