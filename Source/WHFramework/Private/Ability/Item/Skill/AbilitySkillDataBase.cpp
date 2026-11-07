#include "Ability/Item/Skill/AbilitySkillDataBase.h"
#include "Ability/AbilityItemTypes.h"

UAbilitySkillDataBase::UAbilitySkillDataBase()
{
	Type = FName("Skill");
	MaxCount = 1;
	SkillMode = EAbilitySkillMode::None;
}
