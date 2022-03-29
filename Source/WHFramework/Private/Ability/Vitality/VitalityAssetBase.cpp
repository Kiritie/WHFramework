#include "Ability/Vitality/VitalityAssetBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UVitalityAssetBase::UVitalityAssetBase()
{
	Type = UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Vitality);
	MaxCount = 1;
	Range = FVector::OneVector;
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;
	Class = nullptr;
}
