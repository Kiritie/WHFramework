#include "Ability/Character/AbilityCharacterDataBase.h"

#include "Ability/Character/AbilityCharacterInventoryBase.h"

UAbilityCharacterDataBase::UAbilityCharacterDataBase()
{
	Type = FName("Character");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 24.f;
	HalfHeight = 69.f;

	InventoryData = FInventorySaveData();
	InventoryData.InventoryClass = UAbilityCharacterInventoryBase::StaticClass();
	InventoryData.SplitItems.Add(ESlotSplitType::Default, 10);
	InventoryData.SplitItems.Add(ESlotSplitType::Equip, 6);
}
