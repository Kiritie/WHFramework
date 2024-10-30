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
	virtual bool DoAction(const FGameplayTag& InActionTag) = 0;

	virtual bool StopAction(const FGameplayTag& InActionTag) = 0;

	virtual void EndAction(const FGameplayTag& InActionTag, bool bWasCancelled) = 0;

public:
	virtual bool IsPlayer() const = 0;
	
	virtual bool IsEnemy(IAbilityPawnInterface* InTarget) const = 0;

	virtual bool IsMoving() const = 0;

public:
	virtual void GetMotionRate(float& OutMovementRate, float& OutRotationRate) = 0;

	virtual void SetMotionRate(float InMovementRate, float InRotationRate) = 0;

	virtual bool HasActionAbility(const FGameplayTag& InActionTag) const = 0;

	virtual FPawnAbilityActionData GetActionAbility(const FGameplayTag& InActionTag) = 0;

	virtual TMap<FGameplayTag, FPawnAbilityActionData>& GetActionAbilities() = 0;
};
