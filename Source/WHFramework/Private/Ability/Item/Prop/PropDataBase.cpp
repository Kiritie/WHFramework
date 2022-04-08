#include "Ability/Item/Prop/PropDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UPropDataBase::UPropDataBase()
{
	Type = UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Prop);
	MaxCount = 10;
	PropMesh = nullptr;
}
