#include "Ability/Item/Skill/AbilitySkillDataBase.h"

UAbilitySkillDataBase::UAbilitySkillDataBase()
{
	Type = FName("Skill");
	MaxCount = 1;
	SkillMode = EAbilitySkillMode::None;
	ProjectileClass = nullptr;
}
