#include "Asset/AssetModuleTypes.h"

#include "Asset/AssetModuleStatics.h"

FAbilityItem FAbilityItem::Empty = FAbilityItem();

UAbilityItemDataBase& FAbilityItem::GetData(bool bEnsured) const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID, bEnsured);
}

void FDataTableRowBase::OnInitializeRow(const FName& InRowName)
{
	ID = InRowName;
}
