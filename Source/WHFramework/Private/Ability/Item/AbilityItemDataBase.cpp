#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::None);

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
}

EAbilityItemType UAbilityItemDataBase::GetItemType() const
{
	return UAbilityModuleBPLibrary::AssetTypeToItemType(Type);
}
