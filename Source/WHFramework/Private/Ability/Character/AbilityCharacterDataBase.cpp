#include "Ability/Character/AbilityCharacterDataBase.h"

UAbilityCharacterDataBase::UAbilityCharacterDataBase()
{
	Type = FName("Character");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 24.f;
	HalfHeight = 69.f;

	InventoryData = FInventorySaveData();
	InventoryData.SplitItems.Add(ESlotSplitType::Default).Items.SetNum(10);
	InventoryData.SplitItems.Add(ESlotSplitType::Equip).Items.SetNum(6);
}
