#include "Asset/Primary/PrimaryEntityAssetBinding.h"

#include "Asset/AssetManagerBase.h"
#include "Asset/Primary/PrimaryAssetBase.h"

bool FPrimaryEntityAssetBinding::Bind(UObject* InOwner, const FPrimaryAssetId& InAssetId)
{
	if (!IsValid(InOwner)) return false;
	const FPrimaryAssetId RequestedId = InAssetId;
	if (Asset && Asset->GetPrimaryAssetId() == RequestedId) return true;
	Asset = nullptr;
	if (!RequestedId.IsValid()) return false;
	UPrimaryAssetBase* Resolved = UAssetManagerBase::Get().LoadPrimaryAsset(RequestedId, false);
	if (!IsValid(Resolved) || Resolved->GetPrimaryAssetId() != RequestedId)
	{
		UE_LOG(LogTemp, Error, TEXT("实体资产绑定失败：%s"), *RequestedId.ToString());
		return false;
	}
	Asset = Resolved;
	return true;
}

void FPrimaryEntityAssetBinding::Reset()
{
	Asset = nullptr;
}
