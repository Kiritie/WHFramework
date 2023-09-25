#include "Ability/Pawn/AbilityPawnDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"

UAbilityPawnDataBase::UAbilityPawnDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Vitality);
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;

	InventoryData = FInventorySaveData();
	InventoryData.Items.SetNum(5);
	InventoryData.SplitInfos.Add(ESplitSlotType::Default, FSplitSlotInfo(0, 4));

	// tags
	// DeadTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dead");
	// DyingTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dying");
}
