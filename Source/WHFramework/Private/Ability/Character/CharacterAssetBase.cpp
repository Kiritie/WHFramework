#include "Ability/Character/CharacterAssetBase.h"

#include "Ability/AbilityModuleBPLibrary.h"

UCharacterAssetBase::UCharacterAssetBase()
{
	Type = UAbilityModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType::Character);
	MaxCount = 1;
	Range = FVector(1.f, 1.f, 2.f);
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;
	Class = nullptr;
}
