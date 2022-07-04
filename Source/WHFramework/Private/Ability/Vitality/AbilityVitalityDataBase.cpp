#include "Ability/Vitality/AbilityVitalityDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UAbilityVitalityDataBase::UAbilityVitalityDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetTypeByItemType(EAbilityItemType::Vitality);
	MaxCount = 1;
	Range = FVector::OneVector;
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;
	Class = nullptr;

	// tags
	// DeadTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dead");
	// DyingTag = FGameplayTag::RequestGameplayTag("State.Vitality.Dying");
}
