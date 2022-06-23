#pragma once

#include "Ability/Abilities/AbilityBase.h"

#include "VitalityAbilityBase.generated.h"

/**
 * 生命Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UVitalityAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UVitalityAbilityBase();

public:
	UFUNCTION(BlueprintPure)
	class AAbilityVitalityBase* GetOwnerVitality() const;
};