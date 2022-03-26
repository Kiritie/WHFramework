#include "Ability/Item/Skill/SkillAssetBase.h"

#include "Ability/AbilityModuleBPLibrary.h"

USkillAssetBase::USkillAssetBase()
{
	Type = UAbilityModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Skill);
	MaxCount = 1;
	SkillType = ESkillType::None;
	SkillMode = ESkillMode::None;
	SkillClass = nullptr;
}
