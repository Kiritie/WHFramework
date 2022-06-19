#include "Asset/AssetModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"

FAbilityItem FAbilityItem::Empty = FAbilityItem();

UAbilityItemDataBase& FAbilityItem::GetData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID);
}
