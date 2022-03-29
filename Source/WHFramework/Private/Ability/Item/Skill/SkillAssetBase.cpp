#include "Ability/Item/Skill/SkillAssetBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

USkillAssetBase::USkillAssetBase()
{
	Type = UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Skill);
	MaxCount = 1;
	SkillType = ESkillType::None;
	SkillMode = ESkillMode::None;
	SkillClass = nullptr;
}
