#include "Ability/Item/Prop/PropAssetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"

UPropAssetBase::UPropAssetBase()
{
	Type = UAbilityModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Prop);
	MaxCount = 10;
	PropMesh = nullptr;
}
