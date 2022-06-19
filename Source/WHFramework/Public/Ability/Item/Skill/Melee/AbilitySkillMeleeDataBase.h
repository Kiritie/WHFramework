#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"

#include "AbilitySkillMeleeDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilitySkillMeleeDataBase : public UAbilitySkillDataBase
{
	GENERATED_BODY()

public:
	UAbilitySkillMeleeDataBase();
};
