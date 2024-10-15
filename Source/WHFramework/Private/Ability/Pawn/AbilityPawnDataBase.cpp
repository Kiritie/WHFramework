#include "Ability/Pawn/AbilityPawnDataBase.h"

#include "Ability/Pawn/AbilityPawnInventoryBase.h"

UAbilityPawnDataBase::UAbilityPawnDataBase()
{
	Type = FName("Vitality");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;

	InventoryData = FInventorySaveData();
	InventoryData.InventoryClass = UAbilityPawnInventoryBase::StaticClass();
	InventoryData.SplitItems.Add(ESlotSplitType::Default).Items.SetNum(5);
}
