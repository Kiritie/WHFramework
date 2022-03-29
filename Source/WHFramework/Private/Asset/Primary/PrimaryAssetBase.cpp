#include "Asset/Primary/PrimaryAssetBase.h"

UPrimaryAssetBase* UPrimaryAssetBase::Empty = nullptr;

UPrimaryAssetBase::UPrimaryAssetBase()
{
	
}

bool UPrimaryAssetBase::IsValid() const
{
	return GetPrimaryAssetId().IsValid();
}

FPrimaryAssetId UPrimaryAssetBase::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(Type, GetFName());
}
