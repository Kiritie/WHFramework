#include "Ability/Item/ItemDataBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UItemDataBase::UItemDataBase()
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

bool UItemDataBase::EqualType(EAbilityItemType InItemType) const
{
	return Type == UAbilityModuleBPLibrary::GetAssetTypeByItemType(InItemType);
}
