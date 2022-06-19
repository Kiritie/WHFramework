#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityItemDataBase::UAbilityItemDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetTypeByItemType(EAbilityItemType::None);

	Name = FText::GetEmpty();
	Detail = FText::GetEmpty();
	Icon = nullptr;
	Price = 0;
	MaxCount = -1;
	MaxLevel = -1;
	AbilityClass = nullptr;
}

bool UAbilityItemDataBase::EqualType(EAbilityItemType InItemType) const
{
	return Type == UAbilityModuleBPLibrary::GetAssetTypeByItemType(InItemType);
}
