#include "Ability/Item/Skill/AbilitySkillDataBase.h"

UAbilitySkillDataBase::UAbilitySkillDataBase()
{
	Type = FName("Skill");
	MaxCount = 1;
	SkillType = ESkillType::None;
	SkillMode = ESkillMode::None;
	ProjectileClass = nullptr;
}
