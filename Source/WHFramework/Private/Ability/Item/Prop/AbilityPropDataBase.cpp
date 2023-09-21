#include "Ability/Item/Prop/AbilityPropDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"

UAbilityPropDataBase::UAbilityPropDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Prop);
	MaxCount = 10;
	PropClass = nullptr;
	PropPickUpClass = nullptr;
}
