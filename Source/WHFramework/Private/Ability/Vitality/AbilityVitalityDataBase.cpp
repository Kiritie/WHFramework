#include "Ability/Vitality/AbilityVitalityDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityVitalityDataBase::UAbilityVitalityDataBase()
{
	Type = UAbilityModuleBPLibrary::ItemTypeToAssetType(EAbilityItemType::Vitality);
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;

	// tags
	// DeadTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dead");
	// DyingTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dying");
}
