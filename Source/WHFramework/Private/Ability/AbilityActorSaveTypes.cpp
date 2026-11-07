#include "Ability/AbilityActorSaveTypes.h"
#include "Ability/Actor/AbilityActorDataBase.h"

bool FActorSaveData::InitData(const UAbilityActorDataBase* InDefinition, FRandomStream InRandomStream)
{
	if (!::IsValid(InDefinition) || !AssetID.IsValid() || InDefinition->GetPrimaryAssetId() != AssetID) return false;
	Level = InDefinition->ClampLevel(Level);
	if (Name.IsNone())
	{
		Name = FName(*InDefinition->Name.ToString());
	}
	return true;
}
