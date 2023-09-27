#include "Ability/Vitality/AbilityVitalityDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"

UAbilityVitalityDataBase::UAbilityVitalityDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Vitality);
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;

	InventoryData = FInventorySaveData();
	InventoryData.Items.SetNum(5);
	InventoryData.SplitItems.Add(ESlotSplitType::Default).Items.SetNum(5);

	// tags
	// DeadTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dead");
	// DyingTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dying");
}
