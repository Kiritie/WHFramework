#pragma once

#include "Ability/Base/AbilityBase.h"

#include "VitalityAbilityBase.generated.h"

/**
 * Vitality Ability基类
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