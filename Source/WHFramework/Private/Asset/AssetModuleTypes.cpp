#include "Asset/AssetModuleTypes.h"

#include "Asset/AssetModuleBPLibrary.h"

FItem FItem::Empty = FItem();

UItemDataBase* FItem::GetData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAsset<UItemDataBase>(ID);
}
