#include "Ability/Item/ItemDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UItemDataBase* UItemDataBase::Empty = nullptr;

UItemDataBase::UItemDataBase()
{
	Type = UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::None);
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
}

bool UItemDataBase::EqualType(EItemType InItemType) const
{
	return Type == UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(InItemType);
}
