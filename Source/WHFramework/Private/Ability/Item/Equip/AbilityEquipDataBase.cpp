#include "Ability/Item/Equip/AbilityEquipDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"

UAbilityEquipDataBase::UAbilityEquipDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Equip);
	MaxCount = 1;
	EquipClass = nullptr;
	EquipPickUpClass = nullptr;
}
