#include "Asset/Primary/PrimaryAssetBase.h"

UPrimaryAssetBase* UPrimaryAssetBase::Empty = nullptr;

UPrimaryAssetBase::UPrimaryAssetBase()
{
	
}

FPrimaryAssetId UPrimaryAssetBase::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(Type, GetFName());
}
