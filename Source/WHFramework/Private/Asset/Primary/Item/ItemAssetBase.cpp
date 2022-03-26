#include "Asset/Primary/Item/ItemAssetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"

UItemAssetBase* UItemAssetBase::Empty = nullptr;

UItemAssetBase::UItemAssetBase()
{
	Type = UAbilityModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::None);
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
}

FPrimaryAssetId UItemAssetBase::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(Type, GetFName());
}
