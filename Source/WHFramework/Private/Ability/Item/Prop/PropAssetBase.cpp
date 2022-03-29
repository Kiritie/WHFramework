#include "Ability/Item/Prop/PropAssetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UPropAssetBase::UPropAssetBase()
{
	Type = UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Prop);
	MaxCount = 10;
	PropMesh = nullptr;
}
