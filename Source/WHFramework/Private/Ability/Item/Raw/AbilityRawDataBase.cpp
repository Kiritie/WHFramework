#include "Ability/Item/Raw/AbilityRawDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityRawDataBase::UAbilityRawDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Raw);
	MaxCount = 10;
	RawClass = nullptr;
	RawPickUpClass = nullptr;
}
