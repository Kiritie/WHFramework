#include "Asset/AssetModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"

FAbilityItem FAbilityItem::Empty = FAbilityItem();

UAbilityItemDataBase& FAbilityItem::GetData(bool bLogWarning) const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID, bLogWarning);
}
