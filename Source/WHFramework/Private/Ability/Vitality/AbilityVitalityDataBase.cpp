#include "Ability/Vitality/AbilityVitalityDataBase.h"

UAbilityVitalityDataBase::UAbilityVitalityDataBase()
{
	Type = FName("Vitality");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;

	InventoryData = FInventorySaveData();
	InventoryData.SplitItems.Add(ESlotSplitType::Default).Items.SetNum(5);
}
