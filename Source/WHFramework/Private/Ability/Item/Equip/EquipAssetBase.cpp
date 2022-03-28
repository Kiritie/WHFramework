#include "Ability/Item/Equip/EquipAssetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UEquipAssetBase::UEquipAssetBase()
{
	Type = UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Equip);
	MaxCount = 1;
	EquipMesh = nullptr;
	EquipClass = nullptr;
	EffectClass = nullptr;
}
