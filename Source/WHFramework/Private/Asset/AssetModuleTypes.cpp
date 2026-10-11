#include "Asset/AssetModuleTypes.h"

void FDataTableRowBase::OnInitializeRow(const FName& InRowName)
{
	ID = InRowName;
}
