#include "Ability/Item/Skill/SkillDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

USkillDataBase::USkillDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetIDByItemType(EAbilityItemType::Skill);
	MaxCount = 1;
	SkillType = ESkillType::None;
	SkillMode = ESkillMode::None;
	SkillClass = nullptr;
}
