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

	// tags
	// DeadTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dead");
	// DyingTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dying");
	// ActiveTag = FGameplayTag::RequestGameplayTag("State.Character.Active");
	// FallingTag = FGameplayTag::RequestGameplayTag("State.Character.Falling");
	// WalkingTag = FGameplayTag::RequestGameplayTag("State.Character.Walking");
	// JumpingTag = FGameplayTag::RequestGameplayTag("State.Character.Jumping");
}
