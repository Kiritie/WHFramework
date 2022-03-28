#include "Asset/Primary/Item/ItemAssetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UItemAssetBase* UItemAssetBase::Empty = nullptr;

UItemAssetBase::UItemAssetBase()
{
	Type = UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::None);
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
}

bool UItemAssetBase::EqualType(EItemType InItemType) const
{
	return Type == UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(InItemType);
}
