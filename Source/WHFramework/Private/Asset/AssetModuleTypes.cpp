#include "Asset/AssetModuleTypes.h"

#include "Ability/Item/AbilityItemDataBase.h"
#include "Asset/AssetModuleStatics.h"

FAbilityItem FAbilityItem::Empty = FAbilityItem();

UAbilityItemDataBase& FAbilityItem::GetData(bool bEnsured) const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID, bEnsured);
}

bool FAbilityItem::IsDataType(TSubclassOf<UAbilityItemDataBase> InType) const
{
	return UAssetModuleStatics::LoadPrimaryAsset(ID, InType, false) != nullptr;
}

void FDataTableRowBase::OnInitializeRow(const FName& InRowName)
{
	ID = InRowName;
}
