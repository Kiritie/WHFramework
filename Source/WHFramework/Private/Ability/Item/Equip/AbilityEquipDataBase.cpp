#include "Ability/Item/Equip/AbilityEquipDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityEquipDataBase::UAbilityEquipDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Equip);
	MaxCount = 1;
	EquipClass = nullptr;
	EquipPickUpClass = nullptr;
}
