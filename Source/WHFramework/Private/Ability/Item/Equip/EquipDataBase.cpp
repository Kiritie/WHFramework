#include "Ability/Item/Equip/EquipDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UEquipDataBase::UEquipDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetIDByItemType(EAbilityItemType::Equip);
	MaxCount = 1;
	EquipMesh = nullptr;
	EquipClass = nullptr;
	EffectClass = nullptr;
}
