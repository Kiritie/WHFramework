#include "Asset/Primary/PrimaryAssetBase.h"

UPrimaryAssetBase::UPrimaryAssetBase()
{
	
}

void UPrimaryAssetBase::ResetData_Implementation()
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

bool UPrimaryAssetBase::EqualID(const FPrimaryAssetId& InAssetID) const
{
	return GetPrimaryAssetId() == InAssetID;
}
