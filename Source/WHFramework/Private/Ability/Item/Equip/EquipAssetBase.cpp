#include "Ability/Item/Equip/EquipAssetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"

UEquipAssetBase::UEquipAssetBase()
{
	Type = UAbilityModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Equip);
	MaxCount = 1;
	EquipType = EEquipType::Weapon;
	PartType = EEquipPartType::Head;
	EquipMesh = nullptr;
	EquipClass = nullptr;
	EffectClass = nullptr;
}
