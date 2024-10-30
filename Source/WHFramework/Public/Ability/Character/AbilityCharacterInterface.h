#pragma once

#include "Ability/Pawn/AbilityPawnInterface.h"

#include "AbilityCharacterInterface.generated.h"

UINTERFACE()
class WHFRAMEWORK_API UAbilityCharacterInterface : public UAbilityPawnInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityCharacterInterface : public IAbilityPawnInterface
{
	GENERATED_BODY()

public:
	virtual void FreeToAnim() = 0;

	virtual void LimitToAnim() = 0;

	virtual void Swim() = 0;
		
	virtual void UnSwim() = 0;
						
	virtual void Float(float InWaterPosZ) = 0;
						
	virtual void UnFloat() = 0;

	virtual void Climb() = 0;
			
	virtual void UnClimb() = 0;

	virtual void Fly() = 0;

	virtual void UnFly() = 0;

public:
	virtual bool IsFreeToAnim() const = 0;

	virtual bool IsAnimating() const = 0;

	virtual bool IsCrouching(bool bReally = false) const = 0;

	virtual bool IsSwimming(bool bReally = false) const = 0;

	virtual bool IsFloating() const = 0;

	virtual bool IsClimbing() const = 0;

	virtual bool IsFlying(bool bReally = false) const = 0;

public:
	virtual float GetDefaultGravityScale() const = 0;

	virtual float GetDefaultAirControl() const = 0;
};
