#pragma once

#include "Ability/Abilities/AbilityBase.h"

#include "VitalityAbilityBase.generated.h"

class AAbilityVitalityBase;

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
	template<class T>
	T* GetOwnerVitality() const
	{
		return Cast<T>(GetOwningActorFromActorInfo());
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	AAbilityVitalityBase* GetOwnerVitality(TSubclassOf<AAbilityVitalityBase> InClass = nullptr) const;
};
