#include "Asset/AssetModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"

FAbilityItem FAbilityItem::Empty = FAbilityItem();

UItemDataBase* FAbilityItem::GetData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAsset<UItemDataBase>(ID);
}
