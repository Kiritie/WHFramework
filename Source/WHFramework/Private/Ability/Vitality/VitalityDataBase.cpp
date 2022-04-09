#include "Ability/Vitality/VitalityDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UVitalityDataBase::UVitalityDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetIDByItemType(EAbilityItemType::Vitality);
	MaxCount = 1;
	Range = FVector::OneVector;
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;
	Class = nullptr;
}
