#include "Ability/Character/AbilityCharacterDataBase.h"


UAbilityCharacterDataBase::UAbilityCharacterDataBase()
{
	Type = FName("Character");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 24.f;
	HalfHeight = 69.f;
}

