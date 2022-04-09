#include "Ability/Item/Prop/PropDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UPropDataBase::UPropDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetIDByItemType(EAbilityItemType::Prop);
	MaxCount = 10;
	PropMesh = nullptr;
}
