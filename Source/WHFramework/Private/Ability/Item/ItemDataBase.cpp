#include "Ability/Item/ItemDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UItemDataBase::UItemDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetIDByItemType(EAbilityItemType::None);
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
}

bool UItemDataBase::EqualType(EAbilityItemType InItemType) const
{
	return Type == UAbilityModuleBPLibrary::GetAssetIDByItemType(InItemType);
}
