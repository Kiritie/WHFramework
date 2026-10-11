#include "Ability/Pawn/AbilityPawnDataBase.h"


UAbilityPawnDataBase::UAbilityPawnDataBase()
{
	Type = FName("Vitality");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;
}

