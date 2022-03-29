#include "Asset/AssetModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"

FItem FItem::Empty = FItem();

UItemAssetBase* FItem::GetData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAsset<UItemAssetBase>(ID);
}
