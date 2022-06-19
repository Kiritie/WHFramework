#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilitySkillRemoteDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilitySkillRemoteDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilitySkillRemoteDataBase();
};
