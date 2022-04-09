#include "Ability/Character/CharacterDataBase.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"

UCharacterDataBase::UCharacterDataBase()
{
	Type = UAbilityModuleBPLibrary::GetAssetIDByItemType(EAbilityItemType::Character);
	MaxCount = 1;
	Range = FVector(1.f, 1.f, 2.f);
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;
	Class = nullptr;
}
