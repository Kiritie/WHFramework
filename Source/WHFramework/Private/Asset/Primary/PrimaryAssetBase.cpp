#include "Asset/Primary/PrimaryAssetBase.h"

UPrimaryAssetBase::UPrimaryAssetBase()
{
	
}

void UPrimaryAssetBase::OnInitialize_Implementation()
{
}

void UPrimaryAssetBase::OnReset_Implementation()
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
