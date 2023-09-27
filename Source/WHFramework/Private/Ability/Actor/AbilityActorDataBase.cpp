#include "Ability/Actor/AbilityActorDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"

UAbilityActorDataBase::UAbilityActorDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Actor);
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;

	InventoryData = FInventorySaveData();
	InventoryData.Items.SetNum(5);
	InventoryData.SplitItems.Add(ESlotSplitType::Default).Items.SetNum(5);
}
