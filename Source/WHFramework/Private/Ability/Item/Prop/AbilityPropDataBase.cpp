#include "Ability/Item/Prop/AbilityPropDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityPropDataBase::UAbilityPropDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetTypeByItemType(EAbilityItemType::Prop);
	MaxCount = 10;
	PropMesh = nullptr;
}
