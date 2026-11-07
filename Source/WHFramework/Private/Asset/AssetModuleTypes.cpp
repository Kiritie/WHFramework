#include "Asset/AssetModuleTypes.h"
#include "Asset/DataTableTypes.h"

void FDataTableRowBase::OnInitializeRow(const FName& InRowName)
{
	ID = InRowName;
}
