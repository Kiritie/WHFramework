#include "Asset/AssetModuleTypes.h"

#include "Asset/AssetModuleStatics.h"

FAbilityItem FAbilityItem::Empty = FAbilityItem();

UAbilityItemDataBase& FAbilityItem::GetData(bool bLogWarning) const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityItemDataBase>(ID, bLogWarning);
}

void FDataTableRowBase::OnInitializeRow(const FName& InRowName)
{
	ID = InRowName;
}
