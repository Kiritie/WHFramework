#include "Ability/Character/AbilityCharacterDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityCharacterDataBase::UAbilityCharacterDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Character);
	MaxCount = 1;
	Range = FVector(1.f, 1.f, 2.f);
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;
	Class = nullptr;

	// tags
	// DeadTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dead");
	// DyingTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dying");
	// ActiveTag = FGameplayTag::RequestGameplayTag("State.Character.Active");
	// FallingTag = FGameplayTag::RequestGameplayTag("State.Character.Falling");
	// WalkingTag = FGameplayTag::RequestGameplayTag("State.Character.Walking");
	// JumpingTag = FGameplayTag::RequestGameplayTag("State.Character.Jumping");
}
