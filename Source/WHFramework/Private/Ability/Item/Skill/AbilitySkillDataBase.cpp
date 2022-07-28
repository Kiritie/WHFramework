#include "Ability/Item/Skill/AbilitySkillDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilitySkillDataBase::UAbilitySkillDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Skill);
	MaxCount = 1;
	SkillType = ESkillType::None;
	SkillMode = ESkillMode::None;
	SkillClass = nullptr;
	SkillPickUpClass = nullptr;
}
