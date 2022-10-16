#include "Ability/Character/AbilityCharacterDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityCharacterDataBase::UAbilityCharacterDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Character);
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 24.f;
	HalfHeight = 69.f;

	// tags
	// DeadTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dead");
	// DyingTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dying");
	// ActiveTag = FGameplayTag::RequestGameplayTag("State.Character.Active");
	// FallingTag = FGameplayTag::RequestGameplayTag("State.Character.Falling");
	// WalkingTag = FGameplayTag::RequestGameplayTag("State.Character.Walking");
	// JumpingTag = FGameplayTag::RequestGameplayTag("State.Character.Jumping");
}
