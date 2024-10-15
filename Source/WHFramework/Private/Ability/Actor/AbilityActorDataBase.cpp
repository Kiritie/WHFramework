#include "Ability/Actor/AbilityActorDataBase.h"

#include "Ability/Inventory/AbilityInventoryBase.h"

UAbilityActorDataBase::UAbilityActorDataBase()
{
	Type = FName("Actor");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;

	InventoryData = FInventorySaveData();
	InventoryData.InventoryClass = UAbilityInventoryBase::StaticClass();
	InventoryData.SplitItems.Add(ESlotSplitType::Default).Items.SetNum(5);
}
